#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# The sample uses a separate directory for configuration files.
set(APPLICATION_CONFIG_DIR "${CMAKE_CURRENT_LIST_DIR}/configuration")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("Switchable Networks sample")

target_sources(app PRIVATE src/main.c)

add_subdirectory(src/dfu)
add_subdirectory(src/factory_reset)
add_subdirectory(src/network_selector)
add_subdirectory(src/networks)
add_subdirectory(src/ui)
