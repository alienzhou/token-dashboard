"""Read numerical usage metadata only; never persist prompts, code or credentials."""
from __future__ import annotations
import hashlib
import json
import os
import sqlite3
import time
from collections import defaultdict
from contextlib import closing
from datetime import datetime, timezone
from pathlib import Path

NAMES = ('Codex', 'Claude Code', 'Cursor', 'OpenCode', 'Gemini CLI')
DAYS = 364

def fingerprint(value):
    return hashlib.sha256(str(value).encode()).hexdigest()

def count(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or value < 0 or int(value) != value:
        raise ValueError('invalid token count')
    if value > 2**63 - 1:
        raise ValueError('token count exceeds storage range')
    return int(value)

def timestamp(value):
    if isinstance(value, (int, float)):
        return int(value / 1000 if value > 10**11 else value)
    if not isinstance(value, str):
        raise ValueError('missing timestamp')
    dt = datetime.fromisoformat(value.replace('Z', '+00:00'))
    # Agent logs should contain an offset. Reject ambiguous local dates.
    if dt.tzinfo is None:
        raise ValueError('timestamp has no timezone')
    return int(dt.timestamp())

def streaks(days, today):
    active = sorted(day for day, n in days.items() if n > 0 and day <= today)
    longest = run = 0
    prev = None
    for day in active:
        run = run + 1 if prev == day - 1 else 1
        longest = max(longest, run)
        prev = day
    # Today's unused morning does not end yesterday's active streak.
    anchor = today if days.get(today, 0) else today - 1
    current = 0
    while days.get(anchor, 0):
        current += 1
        anchor -= 1
    return longest, current

class Collector:
    def __init__(self, state: Path, home: Path = None):
        self.home = home or Path.home()
        state.parent.mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(str(state))
        os.chmod(state, 0o600)
        self.db.executescript('''
        CREATE TABLE IF NOT EXISTS usage(tool INTEGER, id TEXT, epoch INTEGER, tokens INTEGER,
          PRIMARY KEY(tool,id));
        CREATE TABLE IF NOT EXISTS tasks(id TEXT PRIMARY KEY, seconds INTEGER);
        CREATE TABLE IF NOT EXISTS files(id TEXT PRIMARY KEY, inode INTEGER, offset INTEGER, state TEXT);
        ''')
        self.status = [0, 0, 2, 0, 0]
        self.errors = [0] * 5
        self.new_bytes = 0

    def put(self, tool, event_id, when, tokens):
        n, epoch = count(tokens), timestamp(when)
        # Ignore future/invalid timestamps, instead of inventing a date.
        if epoch < 1577836800 or epoch > time.time() + 300:
            raise ValueError('invalid usage timestamp')
        self.db.execute('INSERT INTO usage VALUES(?,?,?,?) ON CONFLICT(tool,id) DO UPDATE SET tokens=max(usage.tokens,excluded.tokens)',
                        (tool, fingerprint(event_id), epoch, n))

    def codex(self, v, state):
        if v.get('type') == 'session_meta':
            state['session'] = fingerprint(v.get('payload', {}).get('id', ''))
        if v.get('type') != 'event_msg':
            return
        p = v.get('payload', {})
        if not isinstance(p, dict):
            return
        kind = p.get('type')
        if kind == 'task_started' and p.get('turn_id'):
            state['tasks'] = state.get('tasks', {})
            state['tasks'][fingerprint(p['turn_id'])] = timestamp(v['timestamp'])
        if kind == 'task_complete' and p.get('turn_id'):
            key = fingerprint(p['turn_id'])
            start = state.get('tasks', {}).pop(key, None)
            if start:
                duration = timestamp(v['timestamp']) - start
                if 0 <= duration <= 7 * 86400:
                    self.db.execute('INSERT OR REPLACE INTO tasks VALUES(?,?)', (key, duration))
        if kind != 'token_count' or not isinstance(p.get('info'), dict):
            return
        info = p['info']
        usage = info.get('total_token_usage')
        if not isinstance(usage, dict) or 'total_tokens' not in usage:
            return
        total = count(usage['total_tokens'])
        previous = state.get('total', 0)
        epoch = timestamp(v['timestamp'])
        if epoch < 1577836800 or epoch > time.time() + 300:
            raise ValueError('invalid usage timestamp')
        if epoch < state.get('last_epoch', 0):
            return
        state['total'] = total
        state['last_epoch'] = epoch
        if total == previous:
            return
        if total < previous:
            last = info.get('last_token_usage')
            if not isinstance(last, dict) or 'total_tokens' not in last:
                raise ValueError('counter reset without request usage')
            previous = total - count(last['total_tokens'])
        identity = fingerprint((v['timestamp'], total))
        epoch = timestamp(v['timestamp'])
        self.db.execute('INSERT OR IGNORE INTO usage VALUES(?,?,?,?)', (0, identity, epoch, total - previous))

    def claude(self, v, state):
        if v.get('type') != 'assistant':
            return
        m = v.get('message', {})
        if not isinstance(m, dict):
            return
        u = m.get('usage')
        if not isinstance(u, dict) or not m.get('id') or 'input_tokens' not in u or 'output_tokens' not in u:
            return
        n = sum(count(u.get(k, 0)) for k in ('input_tokens', 'output_tokens',
                'cache_read_input_tokens', 'cache_creation_input_tokens'))
        # Multiple content blocks and streaming updates share one API message ID.
        self.put(1, m['id'], v.get('timestamp'), n)

    def gemini_message(self, m):
        if not isinstance(m, dict):
            return
        u = m.get('tokens')
        if m.get('type') != 'gemini' or not m.get('id') or not isinstance(u, dict) or 'total' not in u:
            return
        # Gemini's reported total already includes cached input and thoughts.
        self.put(4, m['id'], m.get('timestamp'), count(u['total']))

    def jsonl(self, path, tool, parser):
        key = fingerprint(path)
        stat = path.stat()
        row = self.db.execute('SELECT inode,offset,state FROM files WHERE id=?', (key,)).fetchone()
        offset, state = (row[1], json.loads(row[2])) if row and row[0] == stat.st_ino and row[1] <= stat.st_size else (0, {})
        bad = state.get('errors', 0)
        with path.open('rb') as f:
            if offset and state.get('anchor'):
                f.seek(max(0, offset - 64))
                if hashlib.sha256(f.read(min(64, offset))).hexdigest() != state['anchor']:
                    offset, state, bad = 0, {}, 0
            f.seek(offset)
            while True:
                start = f.tell()
                line = f.readline()
                if not line:
                    break
                if not line.endswith(b'\n'):
                    f.seek(start)  # an in-progress write will be retried next poll
                    break
                self.new_bytes += len(line)
                try:
                    v = json.loads(line)
                    if isinstance(v, dict):
                        parser(v, state)
                except (ValueError, TypeError, KeyError, OverflowError):
                    bad += 1
            offset = f.tell()
            f.seek(max(0, offset - 64))
            state['anchor'] = hashlib.sha256(f.read(min(64, offset))).hexdigest()
        state['errors'] = bad
        self.errors[tool] += bad
        self.db.execute('INSERT OR REPLACE INTO files VALUES(?,?,?,?)',
                        (key, stat.st_ino, offset, json.dumps(state)))

    def json_file(self, path, tool, parser):
        key = fingerprint(path)
        stat = path.stat()
        row = self.db.execute('SELECT state FROM files WHERE id=?', (key,)).fetchone()
        stamp = [stat.st_mtime_ns, stat.st_size]
        if row and json.loads(row[0]).get('stamp') == stamp:
            return
        self.new_bytes += stat.st_size
        parser(json.loads(path.read_text()))
        self.db.execute('INSERT OR REPLACE INTO files VALUES(?,?,?,?)',
                        (key, stat.st_ino, 0, json.dumps({'stamp': stamp})))

    def opencode_message(self, m, fallback_id=None, fallback_time=None):
        if not isinstance(m, dict) or m.get('role') != 'assistant':
            return
        meta = m.get('metadata') or {}
        assistant = meta.get('assistant') or {}
        u = m.get('tokens') or assistant.get('tokens')
        times = m.get('time') or meta.get('time') or {}
        when = times.get('completed') or times.get('created') or fallback_time
        if not isinstance(u, dict) or 'input' not in u or 'output' not in u:
            return
        n = sum(count(u.get(k, 0)) for k in ('input', 'output', 'reasoning'))
        n += sum(count(u.get('cache', {}).get(k, 0)) for k in ('read', 'write'))
        identity = m.get('id') or fallback_id
        if identity:
            self.put(3, identity, when, n)

    def opencode_db(self, path):
        # No immutable=1: the live WAL must be visible. Never open auth.json.
        with closing(sqlite3.connect(path.resolve().as_uri() + '?mode=ro', uri=True, timeout=1)) as db:
            columns = {r[1] for r in db.execute('PRAGMA table_info(message)')}
            if not {'id', 'data', 'time_updated'}.issubset(columns):
                self.status[3] = 2
                return
            key = fingerprint(path)
            row = self.db.execute('SELECT state,inode FROM files WHERE id=?', (key,)).fetchone()
            watermark = json.loads(row[0]).get('watermark', 0) if row and row[1] == path.stat().st_ino else 0
            end = watermark
            if not db.execute('SELECT 1 FROM message LIMIT 1').fetchone():
                tables = {r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
                if 'session_message' in tables and db.execute('SELECT 1 FROM session_message LIMIT 1').fetchone():
                    self.status[3] = 2  # newer incompatible schema; never invent zero usage
            for identity, data, created, updated in db.execute(
                    'SELECT id,data,time_created,time_updated FROM message WHERE time_updated>=? ORDER BY time_updated', (watermark,)):
                try:
                    self.opencode_message(json.loads(data), identity, created)
                    end = max(end, updated)
                except (ValueError, TypeError, KeyError):
                    self.errors[3] += 1
            self.db.execute('INSERT OR REPLACE INTO files VALUES(?,?,?,?)',
                            (key, path.stat().st_ino, 0, json.dumps({'watermark': end})))

    def scan(self):
        self.new_bytes = 0
        self.errors = [0] * 5
        self.status = [0, 0, 2, 0, 0]
        roots = [(0, self.home / '.codex/sessions', self.codex),
                 (0, self.home / '.codex/archived_sessions', self.codex),
                 (1, self.home / '.claude/projects', self.claude)]
        for tool, root, parser in roots:
            if tool == 0 and self.home == Path.home():
                root = Path(os.environ.get('CODEX_HOME', self.home / '.codex')) / root.name
            for path in sorted(root.rglob('*.jsonl')) if root.exists() else []:
                self.status[tool] = 1
                try:
                    self.jsonl(path, tool, parser)
                except (OSError, ValueError, sqlite3.Error):
                    self.errors[tool] += 1
        root = self.home / '.gemini/tmp'
        for path in sorted(root.glob('*/chats/session-*')) if root.exists() else []:
            if path.suffix not in ('.json', '.jsonl'):
                continue
            self.status[4] = 1
            try:
                if path.suffix == '.jsonl':
                    self.jsonl(path, 4, lambda m, _: self.gemini_message(m))
                else:
                    self.json_file(path, 4, lambda v: [self.gemini_message(m) for m in v.get('messages', [])])
            except (OSError, ValueError, KeyError, TypeError):
                self.errors[4] += 1
        root = self.home / '.local/share/opencode'
        if self.home == Path.home():
            root = Path(os.environ.get('XDG_DATA_HOME', self.home / '.local/share')) / 'opencode'
        database = root / 'opencode.db'
        if database.exists():
            self.status[3] = 1
            try:
                self.opencode_db(database)
            except (OSError, ValueError, sqlite3.Error):
                self.errors[3] += 1
        else:
            for path in sorted((root / 'storage/message').rglob('*.json')) if root.exists() else []:
                self.status[3] = 1
                try:
                    self.json_file(path, 3, self.opencode_message)
                except (OSError, ValueError, TypeError, KeyError):
                    self.errors[3] += 1
        for t in range(5):
            if self.errors[t]:
                self.status[t] = 3
        self.db.commit()

    def snapshot(self, now=None):
        now = int(time.time()) if now is None else int(now)
        today = (now + 28800) // 86400
        sources, all_days = [], defaultdict(int)
        for tool, name in enumerate(NAMES):
            daily = dict(self.db.execute('SELECT (epoch+28800)/86400,sum(tokens) FROM usage WHERE tool=? AND epoch<=? GROUP BY (epoch+28800)/86400', (tool, now)))
            for day, n in daily.items():
                all_days[day] += n
            longest, current = streaks(daily, today)
            sources.append({'name': name, 'status': self.status[tool], 'total': sum(daily.values()),
                            'today': daily.get(today, 0), 'peak': max(daily.values(), default=0),
                            'longest': longest, 'current': current,
                            'days': [daily.get(d, 0) for d in range(today - DAYS + 1, today + 1)]})
        longest, current = streaks(all_days, today)
        return {'updated': now, 'end_day': today, 'total': sum(s['total'] for s in sources),
                'today': all_days.get(today, 0), 'peak': max(all_days.values(), default=0),
                'longest': longest, 'current': current,
                'longest_task': self.db.execute('SELECT coalesce(max(seconds),0) FROM tasks').fetchone()[0],
                'sources': sources, 'days': [all_days.get(d, 0) for d in range(today - DAYS + 1, today + 1)]}
