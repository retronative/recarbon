#!/usr/bin/env python3
# ReCarbon. GPL-3.0-or-later (see COPYING).
#
# Checks performance reports (Settings > Advanced > Export Performance Report;
# reference/PERF_REPORT_FORMAT.md) and prints one line per report, so reports
# from several Macs line up:
#   scripts/perf_report_check.py <report.md>... [--names name,...]
# Each file must hold the separator line and one ```json fence whose document
# has the schema recarbon-perf-report/1, only the known top-level keys, and
# metrics that are well formed (status measured / estimated / unavailable, a
# source, a null value exactly when unavailable, with a why). The whole file
# must pass the denylist: no path, "~/", guest address, this account's home,
# login or full name, this Mac's host name (or --names), or save, settings or
# game file names. An independent reading of the rules in
# runtime/report/perf_scrub.h, not a port of its code. Exit status 1 if any
# file fails.
import getpass, json, os, pwd, re, socket, sys

SCHEMA = 'recarbon-perf-report/1'
SEPARATOR = f'--- machine-readable ({SCHEMA}) ---\n```json\n'
TOP_KEYS = {'schema', 'generated', 'trigger', 'session_s', 'build', 'system', 'display', 'resolution', 'frames',
            'display_events', 'cp', 'gpu', 'hitches', 'thermal', 'cpu', 'warnings', 'flags'}
STATUSES = {'measured', 'estimated', 'unavailable'}
# Leaves allowed outside a metric.
PLAIN = re.compile(r'^\.(schema|generated|trigger|session_s)$|^\.build\.|\.window$|^\.flags\.')
PATH = re.compile(r'(?:^|[\s"\'=(\[:,])/[A-Za-z0-9._-]+/')
GUEST = re.compile(r'(?<![A-Za-z0-9_])(?:0[xX]8[0-9A-Fa-f]{7}|sub_[0-9A-Fa-f]{8})(?![A-Za-z0-9_])')
FILES = ('.sav', 'settings.toml', 'profiles/', '.xex', '.iso')


def fence(text):
    at = text.find(SEPARATOR)
    if at < 0:
        return None
    start = at + len(SEPARATOR)
    end = text.find('\n```', start)
    return None if end < 0 else text[start:end]


def check_metrics(node, path, in_metric, errors):
    if isinstance(node, dict):
        metric = 'status' in node
        if metric:
            if node['status'] not in STATUSES:
                errors.append(f'{path}: status {node["status"]!r}')
            if 'value' not in node:
                errors.append(f'{path}: no value')
            elif (node['value'] is None) != (node['status'] == 'unavailable'):
                errors.append(f'{path}: value {"null" if node["value"] is None else "set"} with status {node["status"]}')
            if not isinstance(node.get('source'), str):
                errors.append(f'{path}: no source')
            if node['status'] == 'unavailable' and 'why' not in node:
                errors.append(f'{path}: unavailable without why')
        for k, v in node.items():
            check_metrics(v, f'{path}.{k}', in_metric or metric, errors)
    elif isinstance(node, list):
        for v in node:
            check_metrics(v, f'{path}[]', in_metric, errors)
    elif not in_metric and not PLAIN.search(path):
        errors.append(f'{path}: a bare value outside a metric')


def private_names(extra):
    """This account's and this Mac's names, as perf_scrub.h's Inputs."""
    whole, words = set(extra), set()
    try:
        whole.add(getpass.getuser())
        gecos = pwd.getpwuid(os.getuid()).pw_gecos
        if gecos:
            words.add(gecos)
            words.update(w for w in re.split(r'[^A-Za-z0-9_]+', gecos) if len(w) >= 3)
    except (KeyError, OSError):
        pass
    host = socket.gethostname()
    whole.update({host, host.split('.')[0]})
    return {n for n in whole | words if len(n) >= 3}


def denylist(text, names, homes):
    if m := PATH.search(text):
        return f'a path near {text[m.start():m.start() + 24]!r}'
    if '~/' in text:
        return 'a ~/ path'
    if GUEST.search(text):
        return 'a guest address'
    for h in homes:
        if len(h) > 1 and h in text:
            return 'the home folder'
    for n in names:
        if re.search(r'(?<![A-Za-z0-9_])' + re.escape(n) + r'(?![A-Za-z0-9_])', text, re.IGNORECASE):
            return 'a user or host name'
    for f in FILES:
        if f in text:
            return f'a file name ({f})'
    return None


def check(path, names, homes):
    """(errors, the document or None)."""
    text = open(path, encoding='utf-8').read()
    errors = []
    body = fence(text)
    if body is None:
        return ['no separator line and ```json fence'], None
    try:
        doc = json.loads(body)
    except json.JSONDecodeError as e:
        return [f'JSON: {e}'], None
    if doc.get('schema') != SCHEMA:
        errors.append(f'schema {doc.get("schema")!r}')
    if set(doc) != TOP_KEYS:
        errors.append(f'top-level keys: unknown {sorted(set(doc) - TOP_KEYS)}, missing {sorted(TOP_KEYS - set(doc))}')
    check_metrics(doc, '', False, errors)
    if why := denylist(text, names, homes):
        errors.append(f'denylist: {why}')
    return errors, doc


def value(doc, *keys):
    for k in keys:
        doc = doc.get(k) if isinstance(doc, dict) else None
    return doc.get('value') if isinstance(doc, dict) else None


def summary(doc):
    mode = value(doc, 'display', 'mode') or {}
    applied = value(doc, 'resolution', 'applied') or {}
    iv = value(doc, 'frames', 'interval_ms') or {}
    fps = value(doc, 'frames', 'fps_mean')
    return (f'{value(doc, "system", "machine")} | {value(doc, "system", "chip")} | {value(doc, "system", "os")} | '
            f'{mode.get("width")}x{mode.get("height")}@{mode.get("hz")} | {value(doc, "resolution", "setting")} -> {applied.get("scale")} | '
            f'{fps} fps p50 {iv.get("p50")} p95 {iv.get("p95")} p99 {iv.get("p99")} max {iv.get("max")} | '
            f'thermal {value(doc, "thermal", "state_now")}')


def main(argv):
    extra = []
    files = []
    it = iter(argv)
    for a in it:
        if a == '--names':
            extra = [n for n in next(it, '').split(',') if n]
        else:
            files.append(a)
    if not files:
        print('usage: perf_report_check.py <report.md>... [--names name,...]', file=sys.stderr)
        return 2
    names = private_names(extra)
    home = os.environ.get('HOME', '').rstrip('/')
    homes = {h for h in (home, os.path.realpath(home) if home else '') if len(h) > 1}
    failed = 0
    for f in files:
        errors, doc = check(f, names, homes)
        if errors:
            failed += 1
            print(f'FAIL {os.path.basename(f)}')
            for e in errors[:20]:
                print(f'  {e}')
        else:
            print(f'PASS {os.path.basename(f)}: {summary(doc)}')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
