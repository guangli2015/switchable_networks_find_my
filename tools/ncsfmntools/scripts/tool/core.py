#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

from abc import ABC, abstractmethod

class Tool(ABC):
        @abstractmethod
        def snr_list_get(self):
                pass

        @abstractmethod
        def memory_read(self, snr, address, size):
                pass
