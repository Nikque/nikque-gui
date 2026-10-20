"""Compares the logs of the command scripts of two runs (for example the test engines in _test/run-b1 and the
distribution in _test/run-dist): the folders of the runs, the engine files and times are made the same first.
usage: py -3 compare_logs.py <old run folder> <new run folder> s1 s2 ...
"""
import re
import sys
from pathlib import Path

SUBS = [
    (re.compile(r'D:\\nikque-gui\\_test\\(run-[^\\]+|engines)'), 'RUN'),
    (re.compile(r'RUN\\engines'), 'RUN'),
    (re.compile(r'wEdax-[\w.-]+\.exe'), 'ENGINE.exe'),
    (re.compile(r'\d+(\.\d+)? ?(秒|s|ms|ミリ秒)\b'), 'T'),
    (re.compile(r'\d+:\d\d(\.\d+)?'), 'T'),
    (re.compile(r'Oct \d+ 2026 [\d:]+'), 'DATE'),
]


def norm(path):
    if not path.exists():
        return None
    out = []
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        for r, s in SUBS:
            line = r.sub(s, line)
        out.append(line)
    return out


def main():
    old, new = Path(sys.argv[1]), Path(sys.argv[2])
    for s in sys.argv[3:]:
        a, b = norm(old / f'{s}-log.txt'), norm(new / f'{s}-log.txt')
        if a is None or b is None:
            print(f'{s}: log missing ({"old" if a is None else "new"})')
            continue
        only_a = [l for l in a if l not in b]
        only_b = [l for l in b if l not in a]
        print(f'{s}: {len(a)} / {len(b)} lines, {len(only_a)} only in old, {len(only_b)} only in new')
        for l in only_a[:8]:
            print('  - ' + l)
        for l in only_b[:8]:
            print('  + ' + l)


if __name__ == '__main__':
    main()
