#!/usr/bin/env python3
"""Exact frame timing for ZGDK games in zeal-native headless console mode (debugger breakpoints).
Breaks on entry to gfx_wait_vblank (game logic done) and gfx_wait_end_vblank (vblank work done),
reads the CPU cycle counter at each, and works out which vblank each frame landed on.
Usage: bpfps.py <game.bin> <seconds> [--enter bk|ld|none] [--pre 'cmd;cmd'] [--counter sym]"""
import os, re, subprocess, sys, argparse, collections
import shlex
from emulator import add_arguments, resolve, boot_commands

ap = argparse.ArgumentParser()
ap.add_argument('bin'); ap.add_argument('secs', type=float)
ap.add_argument('--enter', choices=['bk', 'ld', 'none'], default='none'); ap.add_argument('--pre', default='')
ap.add_argument('--dump', default='')
ap.add_argument('--load', action='store_true')  # also break on gfx_wait_vblank entry to time the game logic
ap.add_argument('--watch', default='')  # sym:len[,sym:len] read at every vblank end
add_arguments(ap)
a = ap.parse_args()
resolve(a, ap)
binp = os.path.realpath(a.bin)
sym = {}
for l in open(binp[:-4] + '.map'):
    m = re.match(r'\s+([0-9A-F]{8})\s+(_gfx_wait_vblank|_gfx_wait_end_vblank)\b', l)
    if m: sym[m.group(2)] = int(m.group(1), 16)
# both are 'in a,(0x9d); bit 1,a; jr cc,loop; xor a; ret': +6 runs once, when the wait ends
WV, WE = sym['_gfx_wait_vblank'] + 6, sym['_gfx_wait_end_vblank'] + 6
cmds = boot_commands(os.path.basename(binp))
if a.enter == 'bk': cmds += ['run 30000000', 'press ENTER', 'run 500000', 'release ENTER', 'run 40000000']
elif a.enter == 'ld': cmds += ['run 60000000', 'tap ENTER', 'run 3000000', 'press ENTER', 'run 3000000', 'release ENTER', 'run 40000000']
else: cmds += ['run 20000000']
if a.pre: cmds += a.pre.split(';')
n = int(a.secs * 60 * 1.02) + 4
watch = []
for w in filter(None, a.watch.split(',')):
    nm, ln = w.split(':')
    for l in open(binp[:-4] + '.map'):
        m = re.match(r'\s+([0-9A-F]{8})\s+%s\b' % nm, l)
        if m: watch.append((nm, int(m.group(1), 16), int(ln)))
rd = ''.join('\nrb 0x%x %d' % (ad, ln) for _, ad, ln in watch)
if a.load:
    W0 = WV - 6
    cmds += ['bp 0x%x' % WV, 'bp 0x%x' % WE] + ['bp 0x%x\nc\nbc 0x%x\nc\nc' % (W0, W0) + rd] * n + ['quit']
else:
    cmds += ['bp 0x%x' % WV, 'bp 0x%x' % WE] + ['c\nc' + rd] * n + ['quit']
env = dict(os.environ)
p = subprocess.run([a.emu, '-r', a.rom,
                    '-H', os.path.dirname(binp), '-o', '-g'] + shlex.split(a.args), input='\n'.join(cmds) + '\n', capture_output=True, text=True, env=env, timeout=3600)
out = p.stdout + p.stderr
ev = [(int(pc, 16), int(c)) for pc, c in re.findall(r'Paused @ 0x([0-9a-f]{4}) \(cyc=(\d+)\)', out)]
# watched bytes after each E break: keep the last rb group seen before each E event
ctx = {}
if watch:
    cur_e = None; buf = []
    for line in out.split('\n'):
        m = re.search(r'Paused @ 0x([0-9a-f]{4}) \(cyc=(\d+)\)', line)
        if m:
            if buf and cur_e is not None: ctx[cur_e] = buf
            buf = []
            cur_e = int(m.group(2)) if int(m.group(1), 16) == WE else cur_e
            continue
        m = re.search(r'\[DBG\] [0-9a-f]{4}: ((?:[0-9a-f]{2} ?)+)$', line.strip())
        if m: buf.append(m.group(1).strip())
    if buf and cur_e is not None: ctx[cur_e] = buf
if len(ev) < n: print((p.stdout + p.stderr)[-2000:]); sys.exit(1)
if a.dump: open(a.dump, 'w').write('\n'.join('%x %d' % e for e in ev))
E = [c for pc, c in ev if pc == WE]; V = [c for pc, c in ev if pc == WV]; L = [c for pc, c in ev if pc == WV - 6]
T = 10_000_000 / 60.0  # nominal; refined from the steady gap below
gaps = [E[i+1] - E[i] for i in range(len(E) - 1)]
T = sorted(gaps)[len(gaps) // 2]
vbl = [round(g / T) for g in gaps]
span = (E[-1] - E[0]) / T
hist = collections.Counter(vbl)
# per emulated second: frames shown vs vblanks elapsed
secs = collections.defaultdict(int)
for i in range(len(E) - 1): secs[int((E[i] - E[0]) / (60 * T))] += 1
# vblank length D = the usual V->E time; a frame whose V->E is shorter noticed vblank late (logic ran into vblank)
pairs = []
j = 0
for i in range(len(E)):
    while j < len(V) and V[j] < (E[i-1] if i else 0): j += 1
    k = j
    while k + 1 < len(V) and V[k+1] < E[i]: k += 1
    if k < len(V) and V[k] < E[i]: pairs.append(E[i] - V[k])
D = collections.Counter(p // 100 * 100 for p in pairs).most_common(1)[0][0] + 100
late_in = [p for p in pairs if p < D - 300]
long = [(round((E[i] - E[0]) / (60 * T), 2), v) + ((' | '.join(ctx.get(E[i], [])),) if watch else ()) for i, v in enumerate(vbl) if v > 1]
print('%s: %.1f s emulated, %d frames over %.0f vblanks -> %.2f fps; vblank period %d T' %
      (os.path.basename(binp), span / 60, len(gaps), span, 60 * len(gaps) / span, T))
print('  frames by vblanks taken:', dict(sorted(hist.items())), ' late frames:', len(long), long[:15] if not watch else '')
if watch and a.dump:
    open(a.dump + '.frames', 'w').write('\n'.join('%.3f %d %s' % ((E[i] - E[0]) / (60 * T), v, ' '.join(ctx.get(E[i], []))) for i, v in enumerate(vbl)))
if False:
    for x in long: print('   ', x)
    bc = collections.defaultdict(lambda: [0, 0])
    for i, v in enumerate(vbl):
        k = ' | '.join(ctx.get(E[i], []))
        bc[k][0] += 1; bc[k][1] += v
    print('  fps by watched state (frames, fps):')
    for k, (f, vb) in sorted(bc.items()): print('    %-40s %5d %6.2f' % (k, f, 60.0 * f / vb))
print('  frames per second:', [secs[k] for k in sorted(secs)][:-1])
print('  vblank length %d T; frames whose logic ran into vblank: %d (worst left %d T of vblank for sprites/HUD)' %
      (D, len(late_in), min(pairs)))

if L:
    import bisect
    ld = {}
    for x in L:
        k = bisect.bisect_left(E, x)
        if 0 < k < len(E): ld[E[k]] = (x - E[k-1])
    ACT = T - D
    r = sorted(v / ACT for v in ld.values())
    print('  logic time per frame vs the %d T between vblanks: median %.2f p90 %.2f p99 %.2f max %.2f; frames over budget %d' %
          (ACT, r[len(r)//2], r[int(len(r)*.9)], r[int(len(r)*.99)], r[-1], sum(x > 1 for x in r)))
    over = [x for x in r if x > 1]
    if over: print('  over-budget frames by how far over: <5%% %d, 5-15%% %d, 15-50%% %d, >50%% %d' % (
        sum(x < 1.05 for x in over), sum(1.05 <= x < 1.15 for x in over), sum(1.15 <= x < 1.5 for x in over), sum(x >= 1.5 for x in over)))
    if a.dump: open(a.dump + '.load', 'w').write('\n'.join('%.3f %.3f' % ((e - E[0]) / (60 * T), v / ACT) for e, v in sorted(ld.items())))
