import json
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'companion'))
from audit_token_usage import audit
from token_dashboard.collector import Collector

class AuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.home = Path(self.temp.name)
        self.state = self.home / 'ledger.sqlite3'
        self.logs = self.home / '.codex/sessions'
        self.logs.mkdir(parents=True)
        records = [
            {'type': 'session_meta', 'payload': {'id': 'synthetic-session'}},
            {'type': 'event_msg', 'timestamp': '2026-06-01T08:00:00Z', 'payload': {
                'type': 'token_count', 'info': {'total_token_usage': {'total_tokens': 100}}}},
            {'type': 'event_msg', 'timestamp': '2026-06-01T08:01:00Z', 'payload': {
                'type': 'token_count', 'info': {'total_token_usage': {'total_tokens': 160}}}},
        ]
        self.path = self.logs / 'synthetic.jsonl'
        self.path.write_text(''.join(json.dumps(v) + '\n' for v in records))
        self.collector = Collector(self.state, self.home)
        self.collector.scan()

    def tearDown(self):
        self.collector.db.close()
        self.temp.cleanup()

    def test_independent_reference_and_panel(self):
        now = 1780387200
        panel = {'snapshot': self.collector.snapshot(now)}
        result = audit(self.state, self.home / '.codex', now, panel)
        self.assertEqual(result['result'], 'PASS')
        self.assertEqual(result['source_tokens'], 160)
        self.assertEqual(result['daily_absolute_difference'], 0)
        self.assertEqual(result['panel_total_difference'], 0)
        # Appended but uncollected bytes cannot change a cursor-bound audit.
        with self.path.open('a') as f:
            f.write('{"not-yet-scanned":true}\n')
        self.assertEqual(audit(self.state, self.home / '.codex', now)['result'], 'PASS')

    def test_detect_tampered_ledger_and_panel(self):
        now = 1780387200
        panel = {'snapshot': self.collector.snapshot(now)}
        panel['snapshot']['sources'][0]['total'] += 1
        self.assertNotEqual(audit(self.state, self.home / '.codex', now, panel)['result'], 'PASS')
        self.collector.db.execute('UPDATE usage SET tokens=tokens+7 WHERE tool=0')
        self.collector.db.commit()
        result = audit(self.state, self.home / '.codex', now)
        self.assertNotEqual(result['result'], 'PASS')
        self.assertEqual(result['mismatched_ledger_records'], 2)
        self.assertEqual(result['daily_absolute_difference'], 14)

    def test_rewrite_and_retained_history_are_explicit(self):
        self.path.write_text('changed\n')
        result = audit(self.state, self.home / '.codex', 1780387200)
        self.assertEqual(result['counts']['changed_files'], 1)
        self.assertNotEqual(result['result'], 'PASS')
        self.path.unlink()
        result = audit(self.state, self.home / '.codex', 1780387200)
        self.assertEqual(result['retained_history_records'], 2)
        self.assertEqual(result['verified_records'], 0)

if __name__ == '__main__':
    unittest.main()
