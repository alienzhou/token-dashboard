"""Run: PYTHONPATH=companion python -m token_dashboard [--no-ble]."""
import argparse
import asyncio
import hashlib
import json
import secrets
import signal
import sys
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from .collector import Collector
from .protocol import SERVICE, RX, ACK, encode, fragments

class WebState:
    def __init__(self):
        self.lock = threading.Lock()
        self.snapshot = None
        self.connection = {'state': 'starting', 'message': '正在读取本地用量', 'last_sync': None}
    def status(self, state, message, synced=None):
        with self.lock:
            self.connection = {**self.connection, 'state': state, 'message': message}
            if synced is not None:
                self.connection['last_sync'] = synced
    def data(self):
        with self.lock:
            return {'snapshot': self.snapshot, 'connection': self.connection}

async def push(client, packet):
    sequence = struct.unpack_from('<I', packet, 4)[0]
    # An authenticated read asks the OS to pair before sending any aggregate.
    await client.read_gatt_char(ACK)
    payload = min(200, max(20, client.mtu_size - 3))
    for chunk in fragments(packet, payload):
        await client.write_gatt_char(RX, chunk, response=True)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        receipt = bytes(await client.read_gatt_char(ACK))
        if len(receipt) == 5 and struct.unpack_from('<I', receipt)[0] == sequence:
            if receipt[4] != 0:
                raise RuntimeError('device rejected snapshot: ' + str(receipt[4]))
            return
        await asyncio.sleep(.1)
    raise TimeoutError('no device receipt')

async def bluetooth(state, device_id=None):
    try:
        from bleak import BleakClient, BleakScanner
    except ImportError:
        state.status('error', '蓝牙依赖未安装，请运行 tools/run-token-dashboard.sh')
        return
    while True:
        try:
            state.status('searching', '寻找 AI Passport；首次连接请长按设备确认键')
            found = await BleakScanner.discover(timeout=5, service_uuids=[SERVICE])
            found = [d for d in found if not device_id or d.address == device_id]
            if not found:
                await asyncio.sleep(5)
                continue
            if len(found) != 1:
                state.status('selection', '发现多台设备，使用 --device 指定目标：' + ', '.join(d.address for d in found))
                await asyncio.sleep(10)
                continue
            async with BleakClient(found[0], timeout=60) as client:
                last_signature, last_sent = None, 0
                state.status('pairing', '等待系统配对，在电脑输入设备屏幕上的六位码')
                while client.is_connected:
                    data = state.data()['snapshot']
                    if data is None:
                        await asyncio.sleep(1)
                        continue
                    signature = hashlib.sha256(json.dumps({k:v for k,v in data.items() if k != 'updated'}, sort_keys=True).encode()).digest()
                    if signature != last_signature or time.monotonic() - last_sent > 60:
                        state.status('syncing', '正在同步统计')
                        await push(client, encode(data, secrets.randbits(32)))
                        last_signature, last_sent = signature, time.monotonic()
                        state.status('connected', '蓝牙已连接 · 自动同步', int(time.time()))
                    await asyncio.sleep(1)
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            # Exception classes give useful diagnostics without exposing log contents.
            state.status('error', '蓝牙连接中断或配对失败（' + type(exc).__name__ + '），5秒后重试')
            await asyncio.sleep(5)

async def collect(state, collector, interval):
    while True:
        try:
            collector.scan()
            data = collector.snapshot()
            with state.lock:
                state.snapshot = data
        except Exception as exc:
            state.status('error', '采集异常（' + type(exc).__name__ + '）；保留上次结果')
        await asyncio.sleep(interval)

def serve(state, port):
    html = Path(__file__).with_name('index.html').read_bytes()
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            host = self.headers.get('Host', '')
            if host not in ('127.0.0.1:' + str(port), 'localhost:' + str(port)):
                self.send_error(403)
                return
            if self.path == '/api/state':
                body, kind = json.dumps(state.data()).encode(), 'application/json; charset=utf-8'
            elif self.path in ('/', '/index.html'):
                body, kind = html, 'text/html; charset=utf-8'
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header('Content-Type', kind)
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'")
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        def log_message(self, *args):
            pass
    server = ThreadingHTTPServer(('127.0.0.1', port), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', type=int, default=8964)
    p.add_argument('--interval', type=float, default=5)
    p.add_argument('--device', help='select a BLE address/UUID when more than one Passport is nearby')
    p.add_argument('--no-ble', action='store_true', help='run local collection and preview only')
    p.add_argument('--once', action='store_true', help='scan once and print aggregate metadata')
    p.add_argument('--state', type=Path, default=Path.home()/'.local/share/token-dashboard/usage.sqlite3')
    args = p.parse_args()
    if not 1 <= args.interval <= 3600:
        p.error('interval must be between 1 and 3600 seconds')
    collector = Collector(args.state)
    if args.once:
        collector.scan()
        print(json.dumps(collector.snapshot(), ensure_ascii=False))
        collector.db.close()
        return
    state = WebState()
    try:
        server = serve(state, args.port)
    except OSError:
        print("面板端口已占用，请使用已有面板或指定 --port。", file=sys.stderr)
        collector.db.close()
        return
    print(f'电脑端面板：http://127.0.0.1:{args.port}（Ctrl+C 退出）', flush=True)
    async def run():
        tasks = [asyncio.create_task(collect(state, collector, args.interval))]
        if not args.no_ble:
            tasks.append(asyncio.create_task(bluetooth(state, args.device)))
        else:
            state.status('preview', '本地实时采集 · 蓝牙未启用')
        try:
            await asyncio.gather(*tasks)
        finally:
            for task in tasks:
                task.cancel()
            await asyncio.gather(*tasks, return_exceptions=True)
    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        pass
    finally:
        server.shutdown()
        collector.db.close()

if __name__ == '__main__':
    main()
