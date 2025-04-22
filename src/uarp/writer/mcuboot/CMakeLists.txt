#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

zephyr_library_include_directories(.)

zephyr_library_sources_ifdef(CONFIG_FMNA_UARP_WRITER_MCUBOOT fmna_uarp_writer_mcuboot.c)

if(CONFIG_FMNA_UARP_WRITER_MCUBOOT)
  zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
endif()
