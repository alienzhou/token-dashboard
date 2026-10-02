#!/usr/bin/env python3
"""Independently reconcile Codex numerical log metadata with a read-only ledger.

No collector parser/reducer is imported. Reports contain aggregate counts only;
source paths, session identifiers and conversation content are never printed.
"""
import argparse
import hashlib
import json
import os
import sqlite3
import time
from collections import Counter
from datetime import datetime
from pathlib import Path
from urllib.parse import urlsplit
from urllib.request import urlopen


def digest(value):
    return hashlib.sha256(str(value).encode()).hexdigest()


def integer(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError('non-numeric count')
    n = int(value)
    if n != value or not 0 <= n <= 2**63 - 1:
        raise ValueError('invalid count')
    return n


def epoch(stamp):
    when = datetime.fromisoformat(stamp.replace('Z', '+00:00'))
    if when.tzinfo is None:
        raise ValueError('ambiguous timestamp')
    return int(when.timestamp())


def audit(state, codex_home, now=None, panel=None):
    now = int(time.time()) if now is None else now
    with sqlite3.connect(state.resolve().as_uri() + '?mode=ro', uri=True) as db:
        db.execute('BEGIN')
        cursors = {k: (inode, offset, json.loads(meta)) for k, inode, offset, meta
                   in db.execute('SELECT id,inode,offset,state FROM files')}
        rows = {identity: (int(when), int(n)) for identity, when, n
                in db.execute('SELECT id,epoch,tokens FROM usage WHERE tool=0 AND epoch<=?', (now,))}
    reference, owners = {}, {}
    counters = Counter()
    for folder in ('sessions', 'archived_sessions'):
        root = codex_home / folder
        for path in sorted(root.rglob('*.jsonl')) if root.exists() else []:
            cursor = cursors.get(digest(path))
            if not cursor:
                counters['unscanned_files'] += 1
                continue
            inode, stop, meta = cursor
            stat = path.stat()
            if stat.st_ino != inode or stat.st_size < stop:
                counters['changed_files'] += 1
                continue
            previous, latest, session = 0, 0, None
            first = True
            with path.open('rb') as file:
                if stop and meta.get('anchor'):
                    file.seek(max(0, stop - 64))
                    if hashlib.sha256(file.read(min(stop, 64))).hexdigest() != meta['anchor']:
                        counters['changed_files'] += 1
                        continue
                    file.seek(0)
                counters['scanned_files'] += 1
                while file.tell() < stop:
                    raw = file.readline(stop - file.tell())
                    if not raw.endswith(b'\n'):
                        counters['incomplete_records'] += 1
                        break
                    try:
                        record = json.loads(raw)
                        if not isinstance(record, dict):
                            continue
                        data = record.get('payload')
                        if not isinstance(data, dict):
                            continue
                        if record.get('type') == 'session_meta':
                            session = digest(data.get('id', ''))
                        if record.get('type') != 'event_msg' or data.get('type') != 'token_count':
                            continue
                        info = data.get('info')
                        if not isinstance(info, dict):
                            continue
                        usage = info.get('total_token_usage')
                        if not isinstance(usage, dict) or 'total_tokens' not in usage:
                            continue
                        total = integer(usage['total_tokens'])
                        stamp = record['timestamp']
                        when = epoch(stamp)
                        if when < 1577836800 or when > now + 300:
                            counters['invalid_records'] += 1
                            continue
                        if when < latest:
                            counters['stale_records'] += 1
                            continue
                        last = info.get('last_token_usage') or {}
                        if first and isinstance(last, dict) and 'total_tokens' in last:
                            counters['initial_cumulative_baselines'] += total > integer(last['total_tokens'])
                        first = False
                        increment = total - previous
                        previous, latest = total, when
                        if not increment:
                            counters['repeated_counters'] += 1
                            continue
                        if increment < 0:
                            counters['counter_resets'] += 1
                            increment = integer(last['total_tokens'])
                        counters['usage_records'] += 1
                        identity = digest((stamp, total))
                        if when > now:
                            continue
                        value = (when, increment)
                        if identity in reference:
                            counters['duplicate_records'] += 1
                            if reference[identity] != value or (owners[identity] and session and owners[identity] != session):
                                counters['identity_conflicts'] += 1
                        else:
                            reference[identity], owners[identity] = value, session
                    except (ValueError, TypeError, KeyError, OverflowError):
                        counters['invalid_records'] += 1
    missing = sum(key not in rows for key in reference)
    mismatches = sum(key in rows and rows[key] != value for key, value in reference.items())
    retained = {key: value for key, value in rows.items() if key not in reference}
    matched_daily, reference_daily = Counter(), Counter()
    for key, (when, n) in reference.items():
        reference_daily[(when + 28800) // 86400] += n
        if key in rows:
            saved_when, saved_n = rows[key]
            matched_daily[(saved_when + 28800) // 86400] += saved_n
    daily_difference = sum(abs(reference_daily[d] - matched_daily[d])
                           for d in set(reference_daily) | set(matched_daily))
    result = {
        'scope': 'Codex available local logs, through ledger byte cursors',
        'counts': dict(counters), 'verified_records': len(reference),
        'missing_ledger_records': missing, 'mismatched_ledger_records': mismatches,
        'daily_absolute_difference': daily_difference,
        'source_tokens': sum(n for _, n in reference.values()),
        'matched_ledger_tokens': sum(rows[key][1] for key in reference if key in rows),
        'retained_history_records': len(retained),
        'retained_history_tokens_unverifiable_from_current_logs': sum(n for _, n in retained.values()),
        'ledger_tokens': sum(n for _, n in rows.values()),
    }
    failures = missing + mismatches + daily_difference + sum(counters[k] for k in (
        'identity_conflicts', 'changed_files', 'unscanned_files', 'invalid_records', 'incomplete_records'))
    if panel is not None:
        data = panel['snapshot']
        cutoff = data['updated']
        daily = Counter()
        for when, n in rows.values():
            if when <= cutoff:
                daily[(when + 28800) // 86400] += n
        source = data['sources'][0]
        today = data['end_day']
        expected = [daily[d] for d in range(today - 363, today + 1)]
        result['panel_total_difference'] = source['total'] - sum(daily.values())
        result['panel_daily_absolute_difference'] = sum(abs(a - b) for a, b in zip(source['days'], expected))
        failures += abs(result['panel_total_difference']) + result['panel_daily_absolute_difference'] + abs(len(source['days']) - 364)
    result['result'] = 'PASS' if not failures else 'FAIL_OR_CHANGED_DURING_AUDIT'
    if not failures and not reference:
        result['result'] = 'NO_CURRENT_RECORDS_TO_VERIFY'
    result['limits'] = 'Verifies arithmetic for recorded metadata, not provider billing or missing history; initial cumulative baselines may predate their first visible log event.'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--state', type=Path, default=Path.home()/'.local/share/token-dashboard/usage.sqlite3')
    parser.add_argument('--codex-home', type=Path, default=Path(os.environ.get('CODEX_HOME', Path.home()/'.codex')))
    parser.add_argument('--url', help='optional local panel URL, e.g. http://127.0.0.1:8964')
    args = parser.parse_args()
    if not args.state.is_file():
        parser.error('ledger does not exist; run the companion first')
    panel = None
    if args.url:
        address = urlsplit(args.url)
        if address.scheme != 'http' or address.hostname not in ('127.0.0.1', 'localhost') or address.username:
            parser.error('panel URL must be a local HTTP address')
        with urlopen(args.url.rstrip('/') + '/api/state', timeout=5) as response:
            panel = json.load(response)
    try:
        result = audit(args.state, args.codex_home, panel=panel)
    except (OSError, sqlite3.Error, ValueError, KeyError, TypeError) as exc:
        result = {'result': 'FAIL_OR_CHANGED_DURING_AUDIT', 'error_type': type(exc).__name__}
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if result['result'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
