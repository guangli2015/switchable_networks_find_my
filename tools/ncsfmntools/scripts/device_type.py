#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

from enum import StrEnum

class DeviceType(StrEnum):
    NRF52832 = 'NRF52832'
    NRF52833 = 'NRF52833'
    NRF52840 = 'NRF52840'
    NRF5340 = 'NRF5340'
    NRF54L15 = 'NRF54L15'
    NRF54H20 = 'NRF54H20'
