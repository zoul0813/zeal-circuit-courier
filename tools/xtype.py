#!/usr/bin/env python3
"""Type text / press keys into the focused X window via XTEST.
Usage: xtype.py "text\n"   |  xtype.py --key Left [--hold 0.5]"""
import sys, time
from Xlib import X, XK, display
from Xlib.ext import xtest

d = display.Display()
SHIFTED = {':': 'colon', '/': 'slash', '.': 'period', '_': 'underscore', '-': 'minus', ' ': 'space', '\n': 'Return'}
NEEDS_SHIFT = set(':_') | set('ABCDEFGHIJKLMNOPQRSTUVWXYZ')

def kc(name):
    return d.keysym_to_keycode(XK.string_to_keysym(name))

def press(name, shift=False, hold=0.05):
    sh = kc('Shift_L')
    if shift: xtest.fake_input(d, X.KeyPress, sh)
    xtest.fake_input(d, X.KeyPress, kc(name)); d.sync(); time.sleep(hold)
    xtest.fake_input(d, X.KeyRelease, kc(name))
    if shift: xtest.fake_input(d, X.KeyRelease, sh)
    d.sync(); time.sleep(0.05)

args = sys.argv[1:]
if args and args[0] == '--key':
    hold = float(args[3]) if len(args) > 3 and args[2] == '--hold' else 0.1
    press(args[1], hold=hold)
else:
    for ch in args[0].encode().decode('unicode_escape'):
        press(SHIFTED.get(ch, ch), ch in NEEDS_SHIFT)
