#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

target_include_directories(app PRIVATE include)
target_sources(app PRIVATE network_selector.c)
zephyr_linker_sources(SECTIONS network_selector_desc.ld)
