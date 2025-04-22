#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

if(NOT FILE_SUFFIX)
  set(FILE_SUFFIX ZDebug CACHE STRING "")
endif()
message(STATUS "File suffix: ${FILE_SUFFIX}")
