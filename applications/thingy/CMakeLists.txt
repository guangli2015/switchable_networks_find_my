#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("Find My Thingy")

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
if(CONFIG_BOARD_THINGY53_NRF5340_CPUAPP OR CONFIG_BOARD_THINGY53_NRF5340_CPUAPP_NS)
  target_sources(app PRIVATE
                     src/platform/motion_poll.c
                     src/platform/speaker_none.c)
endif()
