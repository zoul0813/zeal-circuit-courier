#!/usr/bin/env python3
"""Timeline key driver: drive.py OUTDIR step...  steps: d:Key u:Key w:sec s:name"""
import sys, time, subprocess
from Xlib import X, XK, display
from Xlib.ext import xtest
d = display.Display()
out = sys.argv[1]
def kc(n): return d.keysym_to_keycode(XK.string_to_keysym(n))
for st in sys.argv[2:]:
    op, arg = st.split(":", 1)
    if op == "d": xtest.fake_input(d, X.KeyPress, kc(arg)); d.sync()
    elif op == "u": xtest.fake_input(d, X.KeyRelease, kc(arg)); d.sync()
    elif op == "w": time.sleep(float(arg))
    elif op == "s":
        subprocess.run(["import", "-window", "root", "-crop", "640x480+320+160", "+repage", f"{out}/{arg}.png"]); print(arg)
