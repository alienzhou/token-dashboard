#!/usr/bin/env python3
import asyncio
import json
import sqlite3
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'companion'))
from token_dashboard.collector import Collector, DAYS, streaks
from token_dashboard.protocol import encode, fragments, PACKET_SIZE
from token_dashboard.__main__ import push
NOW = 1780300800
STAMP = '2026-06-01T08:00:00Z'

def event(total, stamp=STAMP):
    return {'type':'event_msg', 'timestamp':stamp, 'payload': {'type':'token_count', 'info': {
            'total_token_usage': {'total_tokens':total, 'cached_input_tokens':total-1}}}}

class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.home = self.root / 'home'
        self.home.mkdir()
        self.c = Collector(self.root / 'state.db', self.home)
    def tearDown(self):
        self.c.db.close()
        self.temp.cleanup()
    def write(self, name, value, lines=False):
        p = self.home / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(''.join(json.dumps(v)+'\n' for v in value) if lines else json.dumps(value))
        return p
    def test_codex_incremental_repeat_copy_and_restart(self):
        records = [event(100), event(100), event(220,'2026-06-01T08:01:00Z')]
        p = self.write('.codex/sessions/a.jsonl', records, True)
        self.write('.codex/archived_sessions/copy.jsonl', records, True)
        self.c.scan(); self.assertEqual(self.c.snapshot(NOW+86400)['total'], 220)
        self.c.scan(); self.assertEqual(self.c.new_bytes,0)
        with p.open('a') as f:
            f.write(json.dumps(event(280,'2026-06-01T08:02:00Z'))[:-2])
        self.c.scan(); self.assertEqual(self.c.snapshot(NOW+86400)['total'],220)
        with p.open('a') as f:
            f.write('}}\n')
        self.c.scan(); self.assertEqual(self.c.snapshot(NOW+86400)['total'],280)
        self.c.db.close(); self.c = Collector(self.root/'state.db', self.home)
        self.c.scan(); self.assertEqual(self.c.snapshot(NOW+86400)['total'],280)
        self.assertEqual(self.c.new_bytes,0)
    def test_reset_and_file_rewrite(self):
        p=self.write('.codex/sessions/a.jsonl',[event(100)],True)
        self.c.scan()
        reset=event(20,'2026-06-01T08:01:00Z')
        reset['payload']['info']['last_token_usage']={'total_tokens':20}
        with p.open('a') as f:f.write(json.dumps(reset)+'\n')
        self.c.scan();self.assertEqual(self.c.snapshot(NOW+86400)['total'],120)
        # Same-inode rewrite must invalidate the cursor, but not duplicate history.
        p.write_text(json.dumps(event(100))+'\n'+json.dumps(reset)+'\n'+json.dumps(event(50,'2026-06-01T08:02:00Z'))+'\n')
        self.c.scan();self.assertEqual(self.c.snapshot(NOW+86400)['total'],150)
    def test_claude_blocks_and_cache(self):
        m = {'type':'assistant','timestamp':STAMP,'message':{'id':'m1', 'usage':{
             'input_tokens':10,'output_tokens':20,'cache_read_input_tokens':30,'cache_creation_input_tokens':40}}}
        self.write('.claude/projects/p/a.jsonl',[m,m],True)
        self.c.scan(); self.assertEqual(self.c.snapshot(NOW+86400)['total'],100)
        m['message']['usage']['output_tokens'] = 25
        p = self.home/'.claude/projects/p/a.jsonl'
        with p.open('a') as f:f.write(json.dumps(m)+'\n')
        self.c.scan(); self.assertEqual(self.c.snapshot(NOW+86400)['total'],105)
    def test_gemini_json_and_jsonl(self):
        m={'type':'gemini','id':'g1','timestamp':STAMP,'tokens':{'input':20,'cached':19,'thoughts':8,'output':9,'total':37}}
        self.write('.gemini/tmp/project/chats/session-one.json',{'messages':[m]})
        self.write('.gemini/tmp/project/chats/session-two.jsonl',[m,m],True)
        self.c.scan(); self.assertEqual(self.c.snapshot(NOW+86400)['total'],37)
        self.c.scan(); self.assertEqual(self.c.new_bytes,0)
    def test_opencode_sqlite_wal_updates(self):
        p=self.home/'.local/share/opencode/opencode.db';p.parent.mkdir(parents=True)
        db=sqlite3.connect(p);db.execute('pragma journal_mode=wal')
        db.execute('create table message(id text, data text, time_created integer, time_updated integer)')
        m={'role':'assistant','tokens':{'input':1,'output':2,'reasoning':3,'cache':{'read':4,'write':5}}}
        db.execute('insert into message values(?,?,?,?)',('o1',json.dumps(m),NOW*1000,NOW*1000));db.commit()
        self.c.scan();self.assertEqual(self.c.snapshot(NOW+86400)['total'],15)
        m['tokens']['output']=9
        db.execute('update message set data=?,time_updated=?',(json.dumps(m),NOW*1000+2));db.commit()
        self.c.scan();self.assertEqual(self.c.snapshot(NOW+86400)['total'],22)
        self.c.scan();self.assertEqual(self.c.snapshot(NOW+86400)['total'],22)
        db.close()
    def test_opencode_legacy_and_missing_states(self):
        m={'id':'old','role':'assistant','metadata':{'time':{'created':NOW*1000},'assistant':{'tokens':{'input':7,'output':8,'reasoning':1,'cache':{'read':2,'write':3}}}}}
        self.write('.local/share/opencode/storage/message/s/a.json',m)
        self.c.scan();s=self.c.snapshot(NOW+86400)
        self.assertEqual(s['total'],21); self.assertEqual([t['status'] for t in s['sources']],[0,0,2,1,0])
    def test_negative_malformed_and_privacy(self):
        self.write('.codex/sessions/a.jsonl',[event(-1),{'prompt':'PRIVATE_PAYLOAD'}],True)
        self.c.scan();s=self.c.snapshot(NOW)
        self.assertEqual(s['sources'][0]['status'],3);self.assertEqual(s['total'],0)
        self.c.scan(); self.assertEqual(self.c.status[0],3)
        self.assertNotIn(b'PRIVATE_PAYLOAD',(self.root/'state.db').read_bytes())
    def test_beijing_dates_and_streaks(self):
        self.c.put(0,'a','2026-06-01T15:59:59Z',10)
        self.c.put(0,'b','2026-06-01T16:00:00Z',20)
        s=self.c.snapshot(NOW+2*86400)
        self.assertEqual(s['days'][-3:-1],[10,20])
        self.assertEqual(streaks({1:2,2:3,4:8,5:1},6),(2,2))
        self.assertEqual(streaks({1:2,2:3},5),(2,0))
    def test_packet_and_fragment_bounds(self):
        s=self.c.snapshot(NOW)
        p=encode(s,123)
        self.assertEqual(len(p),PACKET_SIZE)
        self.assertEqual(struct.unpack_from('<I',p,len(p)-4)[0],zlib.crc32(p[:-4]))
        for payload in [20,180,200]:
            chunks=list(fragments(p,payload))
            self.assertEqual(b''.join(c[6:] for c in chunks),p)
            self.assertTrue(all(len(c)<=payload for c in chunks))
        s['sources'][0]['days'][0]=2**32
        with self.assertRaises(struct.error):encode(s,1)
    def test_transport_ack_and_error(self):
        packet=encode(self.c.snapshot(NOW),44)
        class Fake:
            mtu_size=23
            writes=[]
            async def write_gatt_char(self,uuid,data,response):
                self.writes.append(data)
            async def read_gatt_char(self,uuid):
                return struct.pack('<IB',44,0)
        fake=Fake();asyncio.run(push(fake,packet));self.assertEqual(b''.join(c[6:] for c in fake.writes),packet)
        class Bad(Fake):
            async def read_gatt_char(self,uuid):return struct.pack('<IB',44,1)
        with self.assertRaises(RuntimeError):asyncio.run(push(Bad(),packet))

if __name__=='__main__':
    unittest.main()
