#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

target_include_directories(app PRIVATE include)
target_include_directories(app PRIVATE include_priv)
target_sources_ifdef(CONFIG_APP_DFU app PRIVATE core.c)
target_sources_ifdef(CONFIG_APP_DFU app PRIVATE bt_adv.c)
