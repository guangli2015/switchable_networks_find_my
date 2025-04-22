#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

from . import nv_storage_type as storage_type
from .device_type import DeviceType
from . import tool_type as tool_type

# Sector size for settings partition constraint.
# ZMS could use non-standard sector size, but it is currently not supported
# by the ncsfmntools tool.
SETTINGS_SECTOR_SIZE = 0x1000

NVM_ERASE_VALUE = b'\xff'

DEVICE_DESC = {
    DeviceType.NRF52832: {
        'nvm': {
            'base_address': 0x0,
            'size': 0x80000,
            'write_block_size': 4,
        },
        'default_settings_partition': {
            'relative_address': 0x7e000,
            'size': 0x2000,
            'storage_type': storage_type.NVStorageType.SettingsNVS,
        },
        'tool': tool_type.ToolType.nrfjprog,
    },
    DeviceType.NRF52833: {
        'nvm': {
            'base_address': 0x0,
            'size': 0x80000,
            'write_block_size': 4,
        },
        'default_settings_partition': {
            'relative_address': 0x7e000,
            'size': 0x2000,
            'storage_type': storage_type.NVStorageType.SettingsNVS,
        },
        'tool': tool_type.ToolType.nrfjprog,
    },
    DeviceType.NRF52840: {
        'nvm': {
            'base_address': 0x0,
            'size': 0x100000,
            'write_block_size': 4,
        },
        'default_settings_partition': {
            'relative_address': 0xfe000,
            'size': 0x2000,
            'storage_type': storage_type.NVStorageType.SettingsNVS,
        },
        'tool': tool_type.ToolType.nrfjprog,
    },
    DeviceType.NRF5340: {
        'nvm': {
            'base_address': 0x0,
            'size': 0x100000,
            'write_block_size': 4,
        },
        'default_settings_partition': {
            'relative_address': 0xfc000,
            'size': 0x4000,
            'storage_type': storage_type.NVStorageType.SettingsNVS,
        },
        'tool': tool_type.ToolType.nrfjprog,
    },
    DeviceType.NRF54L15: {
        'nvm': {
            'base_address': 0x0,
            'size': 0x17d000,
            'write_block_size': 16,
        },
        'default_settings_partition': {
            'relative_address': 0x177000,
            'size': 0x6000,
            'storage_type': storage_type.NVStorageType.SettingsZMS,
        },
        'tool': tool_type.ToolType.nrfjprog,
    },
    DeviceType.NRF54H20: {
        'nvm': {
            'base_address': 0x0e000000,
            'size': 0x200000,
            'write_block_size': 16,
        },
        'default_settings_partition': {
            'relative_address': 0x1e3000,
            'size': 0xa000,
            'storage_type': storage_type.NVStorageType.SettingsZMS,
        },
        'tool': tool_type.ToolType.nrfutil,
    },
}
