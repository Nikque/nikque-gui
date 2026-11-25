"""Compares the logs of the command scripts of two runs (for example the test engines in _test/run-b1 and the
distribution in _test/run-dist): the folders of the runs, the engine files and times are made the same first.
usage: py -3 compare_logs.py <old run folder> <new run folder> s1 s2 ...
The lines are compared in order and as many times as each is there. The exit code is 1 when a log differs or is
missing, else 0.
"""
import difflib
import re
import sys
from pathlib import Path

SUBS = [
    (re.compile(r'D:\\nikque-gui\\_test\\(?:[^\\]+\\)?(run-[^\\]+|engines)'), 'RUN'),
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


def differences(a, b):
    """The places where two logs differ, in order: a line that moved, or is there more or fewer times, is one."""
    return [op for op in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes() if op[0] != 'equal']


def main():
    old, new = Path(sys.argv[1]), Path(sys.argv[2])
    differ = 0
    for s in sys.argv[3:]:
        a, b = norm(old / f'{s}-log.txt'), norm(new / f'{s}-log.txt')
        if a is None or b is None:
            print(f'{s}: log missing ({"old" if a is None else "new"})')
            differ += 1
            continue
        ops = differences(a, b)
        if not ops:
            print(f'{s}: {len(a)} lines, the same')
            continue
        differ += 1
        removed = sum(i2 - i1 for _, i1, i2, _, _ in ops)
        added = sum(j2 - j1 for _, _, _, j1, j2 in ops)
        print(f'{s}: {len(a)} / {len(b)} lines, {removed} only in old, {added} only in new, {len(ops)} places')
        for _, i1, i2, j1, j2 in ops[:8]:
            print(f'  @ old line {i1 + 1} ({i2 - i1} lines), new line {j1 + 1} ({j2 - j1} lines)')
            for l in a[i1:i2][:8]:
                print('  - ' + l)
            for l in b[j1:j2][:8]:
                print('  + ' + l)
        if len(ops) > 8:
            print(f'  ... {len(ops) - 8} more places')
    return 1 if differ else 0


if __name__ == '__main__':
    sys.exit(main())
