#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import intelhex
import struct

from enum import Enum

from abc import ABC, abstractmethod

class SectorStatus(Enum):
    ERASED = 0
    OPEN = 1
    CLOSED = 2
    NA = 3

class Settings(ABC):

    def __init__(self, base_addr, sector_size, write_block_size, flash_erase_value, namecnt_id, name_id_offset):
        self.base_addr = base_addr
        self.sector_size = sector_size
        self.write_block_size = write_block_size
        self.flash_erase_value = flash_erase_value

        self.namecnt_id = namecnt_id
        self.name_id_offset = name_id_offset

    def _from_relative_2_absolute_addr(self, offset):
        return self.base_addr + offset

    @staticmethod
    def _validate_kv_records(kv_records):
        for key, value in kv_records.items():
            if not isinstance(key, str):
                print(f"Key {key} is not a string")
                return False
            if not isinstance(value, bytes):
                print(f"Value {value} is not a bytes object")
                return False

        return True

    # Provisioning

    @abstractmethod
    def _write_sector_init_records(self, ih):
        pass

    @abstractmethod
    def _write_single_data_record(self, ih, record_id, data):
        pass

    def _write_single_kv_record(self, ih, key_record_id, key, value):
        value_record_id = key_record_id + self.name_id_offset
        self._write_single_data_record(ih, value_record_id, value)
        self._write_single_data_record(ih, key_record_id, bytes(key, "utf-8"))

        # Update largest name ID ATE:
        name_id_record = struct.pack('<I', key_record_id)
        self._write_single_data_record(ih, self.namecnt_id, name_id_record)

    def write(self, dest_hex_file, kv_records):
        """
        Write key-value records to the Intel Hex file.

        Args:
            dest_hex_file (str): Destination hex file path.
            kv_records (dict): Dictionary with key-value records.

        Returns:
            bool: True if the operation was successful, False otherwise.
        """

        if not self._validate_kv_records(kv_records):
            return False

        ih = intelhex.IntelHex()

        # Write all required records needed to initialize the sector
        self._write_sector_init_records(ih)

        # Write all key-value records
        free_key_record_id = self.namecnt_id + 1
        for key, value in kv_records.items():
            self._write_single_kv_record(ih, free_key_record_id, key, value)
            free_key_record_id += 1

        ih.write_hex_file(dest_hex_file)

        return True

    # Extract

    # Common utility that can be used by child classes
    # to parse a single sector
    def _is_erased_sector(self, sector):
        return sector == (self.sector_size * self.flash_erase_value)

    # Common utility that can be used by child classes
    # to parse a single sector
    def _prepare_sector_records_dict(self, status, records):
        return {"status": status, "records": records}

    def _get_sector_cnt(self, bin_str):
        assert (len(bin_str) % self.sector_size) == 0
        return int(len(bin_str) / self.sector_size)

    def _get_sector_addr_range(self, sector_idx):
        start = self.sector_size * sector_idx
        end = self.sector_size * (sector_idx + 1)
        return start, end

    def _get_sector(self, bin_str, sector_idx):
        start, end = self._get_sector_addr_range(sector_idx)
        sector = bin_str[start:end]
        return sector

    @abstractmethod
    def _parse_sector(self, sector):
        # Each storage type has its own way of parsing the sector
        pass

    def _parse_sectors(self, bin_str):
        metadata = []

        sector_cnt = self._get_sector_cnt(bin_str)
        for i in range(0, sector_cnt):
            sector = self._get_sector(bin_str, i)
            sector_metadata = self._parse_sector(sector)
            metadata.append(sector_metadata)

        return metadata

    def _get_sectors_additional_metadata(self, metadata):
        additional_metadata = {
            "sectors_with_records_cnt": 0,
            "first_sector_with_records_idx": None,
        }

        for idx, sector in enumerate(metadata):
            if sector["records"] is not None:
                if additional_metadata["first_sector_with_records_idx"] is None:
                    additional_metadata["first_sector_with_records_idx"] = idx
                additional_metadata["sectors_with_records_cnt"] += 1

        return additional_metadata

    def _find_newest_sector_id(self, metadata):
        newest_idx = None

        for idx, elem in enumerate(metadata):
            next_elem = metadata[(idx + 1) % len(metadata)]
            if elem["status"] == SectorStatus.CLOSED and \
                next_elem["status"] == SectorStatus.OPEN:
                if newest_idx is not None:
                    print("Memory analysis: Found another transition from closed to open sector")
                newest_idx = (idx + 1) % len(metadata)

        if newest_idx is None:
            print("Memory analysis: There was no transition, will proceed from the end of memory")
            newest_idx = len(metadata) - 1

        return newest_idx

    def _get_settings_range(self, metadata):
        sector_cnt = len(metadata)
        settings_range = None

        additional_metadata = self._get_sectors_additional_metadata(metadata)

        if additional_metadata["sectors_with_records_cnt"] == 0:
            print("Memory analysis: No data records found in the memory")
            return None

        if additional_metadata["sectors_with_records_cnt"] == 1:
            print("Memory analysis: Found only one sector with data records")
            settings_range = (additional_metadata["first_sector_with_records_idx"], additional_metadata["first_sector_with_records_idx"] + 1)
        else:
            print("Memory analysis: Assuming range of settings partition by itself")
            settings_range = (additional_metadata["first_sector_with_records_idx"], sector_cnt)

        print(f"Memory analysis: Search in sector range: {settings_range}")
        return settings_range

    def _order_sectors(self, metadata):
        if len(metadata) == 0:
            print("Memory analysis: No sectors to be analysed")
            return None

        settings_range = self._get_settings_range(metadata)
        if settings_range is None:
            return None

        metadata = metadata[settings_range[0]:settings_range[1]]

        if len(metadata) > 1:
            # Calculate the oldest sector ID
            idx = (self._find_newest_sector_id(metadata) + 1) % len(metadata)
            # Reorder ZMS sectors metadata from the oldest to the newest
            metadata = metadata[idx:] + metadata[:idx]

        return metadata

    def _parse_settings(self, metadata):
        if metadata is None:
            return None

        settings_records = {}

        # Consolidate all records to one dictionary
        consolidated_records = {}
        for sector in metadata:
            if sector["records"] is None:
                continue

            for record_id, value in sector["records"].items():
                consolidated_records[record_id] = value

        # Map settings key to settings value
        for record_id, value in consolidated_records.items():
            if (record_id <= self.namecnt_id) or (record_id >= (self.namecnt_id + self.name_id_offset)):
                # Record ID is not releated to settings key
                continue

            if (value is None) or (len(value) == 0):
                continue

            if value in settings_records:
                print(f"Memory analysis: Found duplicate record with key: {value}")

            settings_value = consolidated_records.get(record_id + self.name_id_offset)
            if (settings_value is None) or (len(settings_value) == 0):
                continue

            key = str(value, "utf-8")
            settings_records[key] = settings_value

        return settings_records

    def read(self, bin_str):
        """
        Read key-value records from the binary string containing the settings partition.

        Args:
            bin_str (str): Binary string containing the settings partition.

        Returns:
            dict: Dictionary with key-value records.
        """

        metadata = self._parse_sectors(bin_str)
        ordered_metadata = self._order_sectors(metadata)
        settings = self._parse_settings(ordered_metadata)

        if settings is None:
            return None

        if not self._validate_kv_records(settings):
            return None

        return settings
