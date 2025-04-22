#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

target_include_directories(app PRIVATE include)
target_include_directories(app PRIVATE include_priv)

target_sources(app PRIVATE
  core.c
  ui_unselected.c
  ui_selected.c
  )

zephyr_linker_sources(DATA_SECTIONS request_listener.ld)
