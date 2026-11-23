#!/usr/bin/env python3
# RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt).
"""Reads dhewm3's RISC OS profile (pairs of PC, LR words; see sys/linux/main.cpp) and prints
where the main thread was, by function (from `nm -n -C` output) and by caller."""
import bisect, collections, struct, sys
prof, syms = sys.argv[1], sys.argv[2]
addrs, names = [], []
for line in open(syms, errors='replace'):
    p = line.rstrip('\n').split(' ', 2)
    if len(p) == 3 and p[1] in 'tTwW':
        addrs.append(int(p[0], 16)); names.append(p[2])
def name(a):
    if a >= 0xFC000000: return '(RISC OS ROM)'
    if a >= 0x20000000 and a < 0x40000000: return '(module/RMA)'
    i = bisect.bisect_right(addrs, a) - 1
    return names[i] if i >= 0 else '(?)'
data = open(prof, 'rb').read()
n = len(data) // 8
pcs = collections.Counter(); pairs = collections.Counter(); lib = collections.Counter()
for i in range(n):
    pc, lr = struct.unpack_from('<II', data, i * 8)
    pcs[name(pc)] += 1
    pairs[(name(pc), name(lr))] += 1
    nm = name(pc)
    lib['Mesa (mgpu_)' if nm.startswith('mgpu_') else 'RISC OS' if nm.startswith('(') else 'UnixLib/libc' if nm.startswith(('__', '_')) else 'game/other'] += 1
print(f'{n} samples')
for k, v in lib.most_common(): print(f'  {100*v/n:5.1f}%  {k}')
print('top functions:')
for k, v in pcs.most_common(30): print(f'  {100*v/n:5.1f}%  {k[:110]}')
print('top (function <- caller):')
for (a, b), v in pairs.most_common(20): print(f'  {100*v/n:5.1f}%  {a[:60]}  <-  {b[:60]}')
