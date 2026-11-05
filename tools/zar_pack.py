#!/usr/bin/env python3
"""Build a game's asset archive (.zar). Called by this game's cmake/assets.cmake.

Files are stored under their own names, which must already be ZAR names (8.3, letters and digits),
because zar.py would silently rename anything else. ZGDK's tileset_chunks() generates tiles.tch and
tiles.ztp before this script runs.

Every run stages into a fresh folder, so a file dropped from the list never lingers in the archive.
Usage: zar_pack.py --out game.zar --zar .../zar.py [file ...]
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

NAME = re.compile(r'^[A-Za-z0-9]{1,8}(\.[A-Za-z0-9]{1,3})?$')


def run(*cmd):
    p = subprocess.run([sys.executable] + list(cmd), capture_output=True, text=True)
    if p.returncode:
        sys.exit('zar_pack: %s failed:\n%s%s' % (os.path.basename(cmd[0]), p.stdout, p.stderr))
    return p.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--zar', required=True)
    ap.add_argument('files', nargs='*')
    a = ap.parse_args()

    stage = tempfile.mkdtemp(prefix='zar_pack_')
    try:
        for f in a.files:
            name = os.path.basename(f)
            if not NAME.match(name):
                sys.exit('zar_pack: %s is not a ZAR name (8.3, letters and digits only)' % name)
            if os.path.exists(os.path.join(stage, name)):
                sys.exit('zar_pack: %s is listed twice' % name)
            shutil.copy(f, os.path.join(stage, name))
        os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
        run(a.zar, '-i', stage, '-o', a.out)
        total = os.path.getsize(a.out)
        if total > 0xFFFF:
            sys.exit('zar_pack: %s is %d bytes; ZAR offsets are 16-bit, so an archive must stay under 64 KB'
                     % (a.out, total))
        names = sorted(os.listdir(stage))
        print('zar_pack: %s, %d bytes: %s' % (os.path.basename(a.out), total,
              ', '.join('%s %d' % (n, os.path.getsize(os.path.join(stage, n))) for n in names)))
    finally:
        shutil.rmtree(stage)


if __name__ == '__main__':
    main()
