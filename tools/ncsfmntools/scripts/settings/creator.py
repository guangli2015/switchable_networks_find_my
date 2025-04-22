#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

from .. import nv_storage_type as storage_type

from . import core as settings_core
from . import nvs_utils as nvs
from . import zms_utils as zms

def settings_instance_create(nv_storage, settings_base, settings_sector_size,   \
                             device_write_block_size, device_flash_erase_value):
    settings_instances = {
        storage_type.NVStorageType.SettingsNVS: nvs.SettingsNVS,
        storage_type.NVStorageType.SettingsZMS: zms.SettingsZMS
    }

    if nv_storage not in settings_instances.keys():
        raise ValueError(f"Invalid NV storage type: {nv_storage}")

    instance = settings_instances[nv_storage](settings_base, settings_sector_size,
                                              device_write_block_size, device_flash_erase_value)

    assert isinstance(instance, settings_core.Settings)

    return instance
