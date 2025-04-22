#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import json
import shlex
import subprocess
from shutil import which

from . import core as tools_core

class ToolNrfutil(tools_core.Tool):
    def __init__(self):
        if which('nrfutil') is None:
            raise Exception('nrfutil is not installed')

    @staticmethod
    def _cmd_run(args, timeout=30):
        cmd = ['nrfutil', '--json', '--skip-overhead'] + args

        try:
            ret = subprocess.run(
                cmd,
                capture_output = True,
                text = True,
                timeout = timeout
            )
        except subprocess.TimeoutExpired:
            raise subprocess.TimeoutExpired(shlex.join(cmd), timeout)

        if ret.returncode:
            print(f'Command "{shlex.join(cmd)}" failed:\n{ret.stderr}')
            raise subprocess.CalledProcessError(ret.returncode, shlex.join(cmd))

        result = json.loads(ret.stdout)

        return result

    def snr_list_get(self):
        args = ['device', 'list']
        result = self._cmd_run(args)

        devices = result['devices']
        snrs = [dev['serialNumber'] for dev in devices if dev['traits']['jlink']]

        return snrs

    def memory_read(self, snr, address, size):
        args = ['device', 'x-read',
                '--serial-number', snr,
                '--address', hex(address),
                '--bytes', hex(size)]
        result = self._cmd_run(args)

        data = result['devices'][0]['memoryData'][0]['values']
        bin_data = bytearray(data)

        return bin_data
