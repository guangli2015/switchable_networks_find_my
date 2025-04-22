#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

target_include_directories(app PRIVATE include)
target_sources(app PRIVATE
  core.c
  fp_adv.c
  )

zephyr_linker_sources(DATA_SECTIONS fp_adv_trigger.ld)

if (NOT CONFIG_BT_FAST_PAIR_FMDN_RING_COMP_NONE)
  target_sources(app PRIVATE ring.c)
endif()
