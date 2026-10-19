import sys
# the board text ("<64> X|O") after the first n moves of a game (passes added)
def flips(me, opp, sq):
    out = 0
    x0, y0 = sq % 8, sq // 8
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            if not dx and not dy: continue
            run = 0; x, y = x0 + dx, y0 + dy
            while 0 <= x < 8 and 0 <= y < 8 and (opp >> (y * 8 + x)) & 1:
                run |= 1 << (y * 8 + x); x += dx; y += dy
            if run and 0 <= x < 8 and 0 <= y < 8 and (me >> (y * 8 + x)) & 1: out |= run
    return out
def moves(me, opp):
    return [s for s in range(64) if not ((me | opp) >> s) & 1 and flips(me, opp, s)]
line, n = sys.argv[1], int(sys.argv[2])
b, w, side = 0x0000000810000000, 0x0000001008000000, 0
import re
mv = [(ord(c) - 97) + 8 * (int(r) - 1) for c, r in re.findall(r'([a-h])([1-8])', line.lower())]
for sq in mv[:n]:
    me, opp = (b, w) if side == 0 else (w, b)
    if not moves(me, opp):
        side ^= 1; me, opp = opp, me
    f = flips(me, opp, sq)
    assert f, line
    me |= f | (1 << sq); opp &= ~f
    b, w = (me, opp) if side == 0 else (opp, me)
    side ^= 1
s = ''.join('X' if (b >> i) & 1 else 'O' if (w >> i) & 1 else '-' for i in range(64))
print(s + (' X' if side == 0 else ' O'))
