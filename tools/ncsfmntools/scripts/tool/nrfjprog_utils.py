#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

from pynrfjprog import LowLevel as API

from . import core as tools_core

class ToolNrfjprog(tools_core.Tool):
    def snr_list_get(self):
        serial_numbers = []
        with API.API(API.DeviceFamily.UNKNOWN) as api:
            serial_numbers = api.enum_emu_snr()
        return serial_numbers

    def memory_read(self, snr, address, size):
        with API.API(API.DeviceFamily.UNKNOWN) as api:
            api.connect_to_emu_with_snr(snr)
            return api.read(address, size)
