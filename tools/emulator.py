"""Emulator configuration shared by this game's development tools."""
import os
import shutil
from pathlib import Path


def add_arguments(parser):
    parser.add_argument('--emu', default=os.environ.get('ZEAL_NATIVE', 'zeal-native'),
                        help='zeal-native executable (default: ZEAL_NATIVE or PATH)')
    parser.add_argument('--args', default='', help='additional emulator options (shell-style quoting)')
    zos = os.environ.get('ZOS_PATH')
    if not zos and os.environ.get('ZDE_PATH'):
        zos = str(Path(os.environ['ZDE_PATH']) / 'home/Zeal-8-bit-OS')
    rom = os.environ.get('ZOS_ROM') or (str(Path(zos) / 'build/os_with_romdisk.img') if zos else None)
    parser.add_argument('--rom', default=rom, help='ZOS ROM image (or set ZOS_ROM/ZOS_PATH/ZDE_PATH)')


def resolve(args, parser):
    executable = shutil.which(args.emu)
    if not executable:
        parser.error('zeal-native not found; pass --emu or set ZEAL_NATIVE')
    if not args.rom or not Path(args.rom).is_file():
        parser.error('ZOS ROM not found; pass --rom or set ZOS_ROM/ZOS_PATH/ZDE_PATH')
    args.emu = executable
    args.rom = str(Path(args.rom).resolve())


def typed(text):
    commands = []
    special = {' ': ['tap SPACE'], ':': ['press SHIFT', 'tap SEMICOLON', 'release SHIFT'],
               '_': ['press SHIFT', 'tap MINUS', 'release SHIFT'], '/': ['tap SLASH'], '.': ['tap PERIOD']}
    for char in text:
        commands += special.get(char, ['tap ' + char.upper()])
        commands.append('run 200000')
    return commands + ['tap ENTER']


def boot_commands(filename):
    # Archives and save files are opened relative to the current ZOS directory.
    return ['run 30000000'] + typed('cd h:/') + ['run 2000000'] + typed('exec h:/' + filename)
