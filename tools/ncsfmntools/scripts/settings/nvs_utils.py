#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import intelhex
import struct

from .. import device as DEVICE
from . import core as settings_core

class ATE:
    # Minimal length of the ATE without adjustment for the write block size
    _ATE_LEN_MIN = 8

    # CRC8 CCITT table
    _CRC8_CCITT_TABLE = [
        0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
        0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d]

    def __init__(self, bytes, record_id, data_offset, data_len, crc, write_block_size):
        assert len(bytes) == ATE.size_from_write_block_size(write_block_size)

        self.bytes = bytes
        self.record_id = record_id
        self.data_offset = data_offset
        self.data_len = data_len
        self.crc = crc

    @classmethod
    def size_from_write_block_size(cls, write_block_size):
        assert write_block_size > 0
        assert (write_block_size & (write_block_size - 1)) == 0

        ate_len = cls._ATE_LEN_MIN

        return (ate_len + (write_block_size - 1)) & ~(write_block_size - 1)

    @classmethod
    def crc8_ccitt_from_bytes(cls, buf):
        cnt = len(buf)
        crc = 0xff
        for i in range(cnt):
            crc = (crc ^ buf[i]) & 0xff
            crc = ((crc << 4) ^ cls._CRC8_CCITT_TABLE[(crc >> 4) & 0xff]) & 0xff
            crc = ((crc << 4) ^ cls._CRC8_CCITT_TABLE[(crc >> 4) & 0xff]) & 0xff

        return crc

    @classmethod
    def deserialize(cls, bytes, write_block_size):
        assert len(bytes) == ATE.size_from_write_block_size(write_block_size)

        vals = struct.unpack('<HHHBB', bytes[:8])
        record_id, data_offset, data_len, _, crc = vals
        return cls(bytes, record_id, data_offset, data_len, crc, write_block_size)

    @classmethod
    def serialize(cls, record_id, data_offset, data_len, write_block_size):
        ate_size = ATE.size_from_write_block_size(write_block_size)
        ate = bytearray(DEVICE.NVM_ERASE_VALUE * ate_size)

        ate[0:6] = struct.pack('<HHH', record_id, data_offset, data_len)
        ate[7] = cls.crc8_ccitt_from_bytes(ate[:7])

        return bytes(ate)

    def _crc8_ccitt_calculate(self):
        ate_without_crc = self.bytes[:7]
        cnt = len(ate_without_crc)

        return ATE.crc8_ccitt_from_bytes(ate_without_crc)

    def is_valid(self):
        if (self._crc8_ccitt_calculate() == self.crc) \
            and self.data_offset < (DEVICE.SETTINGS_SECTOR_SIZE - self.size()):
            return True

        return False

    def size(self):
        return len(self.bytes)

    def is_populated(self):
        return self.bytes != (DEVICE.NVM_ERASE_VALUE * self.size())

    def __str__(self):
        if not self.is_populated():
            return 'ATE: unpopulated'

        return (f'ATE: {"valid" if self.is_valid() else "invalid"}\n'
                f'  Record ID: {hex(self.record_id)}\n'
                f'  Data offset: {hex(self.data_offset)}\n'
                f'  Data length: {self.data_len}'
                f'  Bytes: {self.bytes.hex()}')


class SettingsNVS(settings_core.Settings):

    NAMECNT_ID = 0x8000
    NAME_ID_OFFSET = 0x4000

    def __init__(self, base_addr, sector_size, write_block_size, flash_erase_value):
        super().__init__(base_addr,
                         sector_size,
                         write_block_size,
                         flash_erase_value,
                         self.NAMECNT_ID,
                         self.NAME_ID_OFFSET)

        ate_size = ATE.size_from_write_block_size(write_block_size)

        # Start from the last ATE.
        self.ate_offset = (self.sector_size - ate_size)
        self.data_offset = 0

    # Provisioning

    def _align_nvs_data(self, data):
        padding = (self.write_block_size - len(data)) % self.write_block_size
        return data + self.flash_erase_value * padding

    def _verify_sector(self):
        # Multi sector provisioning is not supported.
        assert self.ate_offset >= self.data_offset

    def _change_sector_ate_offset(self, ate_len):
        self.ate_offset -= ate_len
        self._verify_sector()

    def _change_sector_data_offset(self, data_len):
        self.data_offset += data_len
        self._verify_sector()

    def _write_sector_init_records(self, ih):
        # Skip the latest ATE for the close ATE
        ate_size = ATE.size_from_write_block_size(self.write_block_size)
        self._change_sector_ate_offset(ate_size)

        # Now the sector is ready for data ATEs.

    def _write_single_data_record(self, ih, record_id, data):
        ate = ATE.serialize(record_id, self.data_offset, len(data), self.write_block_size)
        ih.frombytes(ate, self._from_relative_2_absolute_addr(self.ate_offset))
        self._change_sector_ate_offset(len(ate))

        aligned_data = self._align_nvs_data(data)
        ih.frombytes(aligned_data, self._from_relative_2_absolute_addr(self.data_offset))
        self._change_sector_data_offset(len(aligned_data))

    # Extraction

    def _is_close_ate(self, ate):
        if (not ate.is_valid()) or (ate.record_id != 0xffff) or (ate.data_len != 0):
            return False

        if (DEVICE.SETTINGS_SECTOR_SIZE - ate.data_offset) % ate.size() != 0:
            return False

        return True

    def _parse_sector(self, sector):
        data_ptr = 0
        records = {}
        sector_status = settings_core.SectorStatus.NA

        if self._is_erased_sector(sector):
            # If erased, there is nothing else to check
            return self._prepare_sector_records_dict(settings_core.SectorStatus.ERASED, None)

        ate_size = ATE.size_from_write_block_size(self.write_block_size)
        ate_ptr = len(sector) - ate_size
        ate = ATE.deserialize(sector[ate_ptr:(ate_ptr + ate_size)],
                                self.write_block_size)

        if self._is_close_ate(ate):
            sector_status = settings_core.SectorStatus.CLOSED
        elif not ate.is_populated():
            sector_status = settings_core.SectorStatus.OPEN
        else:
            # NVS sector shall have erase value
            # or close ATE at the end
            return self._prepare_sector_records_dict(settings_core.SectorStatus.NA, None)

        while ate_ptr >= 0:
            ate_ptr = ate_ptr - ate_size

            if ate_ptr < data_ptr:
                # ATE would be already in the data segment of the sector,
                # ending analysis in this sector
                break

            ate = ATE.deserialize(sector[ate_ptr:(ate_ptr + ate_size)],
                                    self.write_block_size)

            if not ate.is_populated():
                # No more ATE, ending analysis in this sector
                break

            if not ate.is_valid():
                # Invalid ATE, move to the next one
                continue

            if ate.data_offset < data_ptr:
                # Invalid data offset, it cannot point to the offset in
                # the scope of the previous ATE
                return self._prepare_sector_records_dict(settings_core.SectorStatus.NA, None)

            if (ate.data_offset + ate.data_len) >= ate_ptr:
                # Invalid data len, it cannot point to the offset
                # where ATE records are already placed
                return self._prepare_sector_records_dict(settings_core.SectorStatus.NA, None)

            # We assume that the data is correct, dictionary is used to preserve the newest record
            records[ate.record_id] = sector[ate.data_offset:(ate.data_offset + ate.data_len)]
            data_ptr = ate.data_offset + ate.data_len

        # If classified as open but there was not even one ATE record or
        # it failed data offset or len check, it is not considered as the NVS sector
        if sector_status == settings_core.SectorStatus.OPEN and len(records) == 0:
            return self._prepare_sector_records_dict(settings_core.SectorStatus.NA, None)

        return self._prepare_sector_records_dict(sector_status, records)
