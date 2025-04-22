#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import os
import sys
import argparse
import importlib

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from __package_info__ import __version__ as version

commands = [
    ('provision', '..cmd_provision', 'FMN Accessory Setup Provisioning Tool'),
    ('extract', '..cmd_extract', 'FMN Accessory MFi Token Extractor Tool'),
    ('superbinary', '..cmd_superbinary', 'FMN SuperBinary Helper Tool')
]


def cli():
    argv = sys.argv.copy()

    if len(argv) > 1:
        cmd = argv[1].lower()
        argv = argv[2:]
        for command in commands:
            if cmd == command[0].lower():
                mod = importlib.import_module(command[1], __name__)
                return (mod.cli)(command[0], argv)

    print('Find My CLI Tools for nRF Connect SDK v' + version)
    print('')
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('command [--help]')
    parser.add_argument('parameter', nargs='*')
    parser.print_usage()
    print('')
    print('commands:')
    max_len = 0
    for command in commands:
        max_len = max(max_len, len(command[0]))
    for command in commands:
        print(f'  {(command[0] + (" " * max_len))[0:max_len]}   - {command[2]}')

    exit(1)

if __name__ == '__main__':
    cli()
