#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

zephyr_library_include_directories(.)
zephyr_library_sources(fmna_uarp_payload.c)

zephyr_linker_sources(SECTIONS fmna_uarp_payload.ld)

add_subdirectory(mcuboot_app)
