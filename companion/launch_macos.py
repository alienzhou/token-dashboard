"""Launch via LaunchServices so Bluetooth permission belongs to the app."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time


def main():
    with tempfile.TemporaryDirectory(prefix='token-dashboard-launch-') as directory:
        pid_file = Path(directory) / 'pid'
        log_file = Path(directory) / 'output'
        log_file.touch()
        stopping = False
        stop_sent = False
        stop_deadline = None

        def request_stop(signum, frame):
            nonlocal stopping
            stopping = True

        signal.signal(signal.SIGINT, request_stop)
        signal.signal(signal.SIGTERM, request_stop)
        # Executing Contents/MacOS directly inherits the terminal's TCC
        # identity. In Warp that aborts despite the app's usage description.
        process = subprocess.Popen([
            '/usr/bin/open', '-n', '-g', '-W', '-a',
            str(Path(sys.argv[1]).resolve().parents[2]),
            '--stdout', str(log_file), '--stderr', str(log_file),
            '--env', 'TOKEN_DASHBOARD_LAUNCH_PID=' + str(pid_file),
            '--args', *sys.argv[2:],
        ])
        with log_file.open('rb') as output:
            while True:
                chunk = output.read()
                if chunk:
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.buffer.flush()
                if stopping and pid_file.exists():
                    try:
                        pid = int(pid_file.read_text())
                        if not stop_sent:
                            os.kill(pid, signal.SIGINT)
                            stop_sent = True
                            stop_deadline = time.monotonic() + 10
                        elif time.monotonic() >= stop_deadline:
                            os.kill(pid, signal.SIGTERM)
                    except (FileNotFoundError, ProcessLookupError, ValueError):
                        pass
                result = process.poll()
                if result is not None:
                    sys.stdout.buffer.write(output.read())
                    sys.stdout.buffer.flush()
                    return result
                time.sleep(.1)


if __name__ == '__main__':
    sys.exit(main())
