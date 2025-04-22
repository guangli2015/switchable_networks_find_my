#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import binascii
import intelhex
import struct
import zlib

from dataclasses import dataclass
from typing import Union, Any

from . import core as settings_core

# Used for special ATEs, such as: Empty, Close, GC done.
@dataclass
class Metadata:
    metadata: int    # uint32_t metadata;

    def __eq__(self, other):
        if not isinstance(other, Metadata):
            return False

        return self.metadata == other.metadata

    def serialize(self):
        return struct.pack('<I', self.metadata)

    @classmethod
    def deserialize(cls, data):
        return cls(metadata=struct.unpack('<I', data)[0])

# Used for data ATEs that point to the big data.
@dataclass
class DataCRC:
    data_crc: int   # uint32_t data_crc;

    def __eq__(self, other):
        if not isinstance(other, DataCRC):
            return False

        return self.data_crc == other.data_crc

    @staticmethod
    def crc32_from_bytes(buf):
        return zlib.crc32(buf) & 0xffffffff

    def verify_crc32(self, data):
        # CRC32 support is optional
        if self.data_crc == 0:
            return True

        if self.data_crc != self.crc32_from_bytes(data):
            print("Memory analysis: CRC32 mismatch")
            return False

        return True

    def serialize(self):
        return struct.pack('<I', self.data_crc)

    @classmethod
    def deserialize(cls, data):
        return cls(data_crc=struct.unpack('<I', data)[0])

# Used for data ATEs that contain small data.
@dataclass
class SmallData:
    data: bytes     # uint8_t data[8];

    MAX_SIZE = 8

    def __eq__(self, other):
        if not isinstance(other, SmallData):
            return False

        return self.data == other.data

    def serialize(self):
        assert len(self.data) == self.MAX_SIZE
        return self.data

    @classmethod
    def deserialize(cls, data):
        assert len(data) == cls.MAX_SIZE
        return cls(data=data)

# Used for either data ATEs that either points to the big data or for special ATEs.
@dataclass
class DataInfo:
    offset: int                                 # uint32_t offset;
    additional_info: Union[DataCRC, Metadata]   # union {
                                                #   uint32_t data_crc;
                                                #   uint32_t metadata;
                                                 # };

    def __eq__(self, other):
        if not isinstance(other, DataInfo):
            return False

        return (self.offset == other.offset) and \
               (self.additional_info == other.additional_info)

    def serialize(self):
        return struct.pack('<I', self.offset) + self.additional_info.serialize()

    @classmethod
    def deserialize(cls, data, is_special_ate):
        offset = struct.unpack('<I', data[:4])[0]
        additional_info = None
        if is_special_ate:
            additional_info = Metadata.deserialize(data[4:8])
        else:
            additional_info = DataCRC.deserialize(data[4:8])
        return cls(offset=offset, additional_info=additional_info)

# Represents the ZMS ATE:
#
# struct zms_ate {
#     uint8_t crc8;
#     uint8_t cycle_cnt;
#     uint16_t len;
#     uint32_t id;
#     union {
#         uint8_t data[8];
#         struct {
#             uint32_t offset;
#             union {
#                 uint32_t data_crc;
#                 uint32_t metadata;
#             };
#         };
#     };
# } __packed;
@dataclass
class AteZMS:
    cycle_cnt: int
    len: int
    id: int
    content: Union[SmallData, DataInfo]

    crc8: int = 0xff

    auto_update_crc8: bool = True   # if True, the CRC8 will be updated during serialization
                                    # and __post_init__

    # Size of the ZMS ATE equal to sizeof(struct zms_ate).
    ATE_SIZE = 16

    # Special ATE ID, used by Empty and Close ATEs.
    HEAD_ID = 0xffffffff

    def __post_init__(self):
        if self.auto_update_crc8:
            self.crc8 = self.crc8_ccitt_from_bytes(self.serialize()[1:])

    def __eq__(self, other):
        if not isinstance(other, AteZMS):
            return False

        return (self.id == other.id) and \
               (self.len == other.len) and \
               (self.content == other.content) and \
               (self.cycle_cnt == other.cycle_cnt) and \
               (self.crc8 == other.crc8)

    @staticmethod
    def crc8_ccitt_from_bytes(buf):
        CRC8_CCITT_TABLE = [
            0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
            0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d]

        cnt = len(buf)
        crc = 0xff
        for i in range(cnt):
            crc = (crc ^ buf[i]) & 0xff
            crc = ((crc << 4) ^ CRC8_CCITT_TABLE[(crc >> 4) & 0xff]) & 0xff
            crc = ((crc << 4) ^ CRC8_CCITT_TABLE[(crc >> 4) & 0xff]) & 0xff

        return crc

    def serialize(self):
        header_data = struct.pack('<BHI', self.cycle_cnt, self.len, self.id)
        content_data = self.content.serialize()

        body = header_data + content_data

        if self.auto_update_crc8:
            self.crc8 = self.crc8_ccitt_from_bytes(body)
        header_crc8 = struct.pack('<B', self.crc8)

        ate = header_crc8 + body

        return ate

    def is_valid_crc8(self):
        # Skip the CRC8 byte.
        data_to_validate = self.serialize()[1:]

        return self.crc8 == self.crc8_ccitt_from_bytes(data_to_validate)

    def is_valid_cycle_cnt(self, cycle_cnt):
        return self.cycle_cnt == cycle_cnt

    def is_valid(self, cycle_cnt):
        return self.is_valid_crc8() and self.is_valid_cycle_cnt(cycle_cnt)

    @classmethod
    def deserialize(cls, ate_bytes, write_block_size):
        assert len(ate_bytes) == AteZMS.size_from_write_block_size(write_block_size)

        ate_crc8, ate_cycle_cnt, ate_len, ate_id = struct.unpack('<BBHI', ate_bytes[:8])

        is_special_ate = (ate_id == AteZMS.HEAD_ID)

        if ate_len <= SmallData.MAX_SIZE and not is_special_ate:
            content = SmallData.deserialize(ate_bytes[8:16])
        else:
            content = DataInfo.deserialize(ate_bytes[8:16], is_special_ate)

        return cls(id=ate_id,
                   len=ate_len,
                   content=content,
                   cycle_cnt=ate_cycle_cnt,
                   crc8=ate_crc8,
                   auto_update_crc8=False)


    @classmethod
    def size_from_write_block_size(cls, write_block_size):
        assert write_block_size > 0
        assert (write_block_size & (write_block_size - 1)) == 0

        return (cls.ATE_SIZE + (write_block_size - 1)) & ~(write_block_size - 1)

# Special ATE: Empty ATE, located at the end of the sector (N ate).
class EmptyAteZMS(AteZMS):
    ZMS_VERSION = 0x01
    ZMS_VERSION_OFFSET = 0

    ZMS_MAGIC = 0x42
    ZMS_MAGIC_OFFSET = 8

    ZMS_RESERVED = 0x0000
    ZMS_RESERVED_OFFSET = 16

    @classmethod
    def _serialize_metadata(cls):
        return (cls.ZMS_VERSION << cls.ZMS_VERSION_OFFSET) | \
               (cls.ZMS_MAGIC << cls.ZMS_MAGIC_OFFSET) | \
               (cls.ZMS_RESERVED << cls.ZMS_RESERVED_OFFSET)

    def __init__(self, cycle_cnt):
        metadata = Metadata(metadata=EmptyAteZMS._serialize_metadata())
        content = DataInfo(offset=0x0, additional_info=metadata)
        super().__init__(id=self.HEAD_ID,
                         len=0xffff,
                         content=content,
                         cycle_cnt=cycle_cnt)

# Special ATE: Close ATE, located at the end of the sector (N-1 ate).
class CloseAteZMS(AteZMS):
    def __init__(self, cycle_cnt, offset):
        metadata = Metadata(metadata=0xffffffff)
        content = DataInfo(offset=offset, additional_info=metadata)
        super().__init__(id=self.HEAD_ID,
                 len=0x0,
                 content=content,
                 cycle_cnt=cycle_cnt)

class SettingsZMS(settings_core.Settings):

    NAMECNT_ID = 0x80000000
    NAME_ID_OFFSET = 0x40000000

    def __init__(self, base_addr, sector_size, write_block_size, flash_erase_value):
        super().__init__(base_addr,
                 sector_size,
                 write_block_size,
                 flash_erase_value,
                 self.NAMECNT_ID,
                 self.NAME_ID_OFFSET)

        ate_size = AteZMS.size_from_write_block_size(write_block_size)

        # Start from the last ATE.
        # All required ATEs will be firstly written to the end of the sector.
        self.ate_offset = (self.sector_size - ate_size)
        self.data_offset = 0

    # Provisioning

    def _verify_sector(self):
        # Multi sector provisioning is not supported.
        assert self.ate_offset >= self.data_offset

    def _change_sector_ate_offset(self, ate_len):
        self.ate_offset -= ate_len
        self._verify_sector()

    def _change_sector_data_offset(self, data_len):
        self.data_offset += data_len
        self._verify_sector()

    def _align_data_small(self, data):
        assert len(data) <= SmallData.MAX_SIZE
        return data + self.flash_erase_value * (SmallData.MAX_SIZE - len(data))

    def _align_data_to_wbs(self, data):
        padding = (self.write_block_size - len(data)) % self.write_block_size
        return data + self.flash_erase_value * padding

    def _write_zms_ate(self, ih, zms_ate):
        serialized_ate = self._align_data_to_wbs(zms_ate.serialize())
        ih.frombytes(serialized_ate, self._from_relative_2_absolute_addr(self.ate_offset))
        self._change_sector_ate_offset(len(serialized_ate))

    def _write_init_empty_ate(self, ih):
        empty_ate = EmptyAteZMS(cycle_cnt=0x01)
        self._write_zms_ate(ih, empty_ate)

    def _write_data_big(self, ih, data):
        aligned_data = self._align_data_to_wbs(data)
        ih.frombytes(aligned_data, self._from_relative_2_absolute_addr(self.data_offset))
        self._change_sector_data_offset(len(aligned_data))

    def _write_data_record_big(self, ih, record_id, data):
        data_crc = DataCRC(data_crc=DataCRC.crc32_from_bytes(data))
        content = DataInfo(offset=self.data_offset, additional_info=data_crc)
        zms_ate = AteZMS(id=record_id,
                 len=len(data),
                 content=content,
                 cycle_cnt=0x01)

        self._write_zms_ate(ih, zms_ate)
        self._write_data_big(ih, data)

    def _write_data_record_small(self, ih, record_id, data):
        content = SmallData(data=self._align_data_small(data))
        zms_ate = AteZMS(id=record_id,
                 len=len(data),
                 content=content,
                 cycle_cnt=0x01)

        self._write_zms_ate(ih, zms_ate)

    def _write_sector_init_records(self, ih):
        # Write the empty ATE to the end of the sector.
        self._write_init_empty_ate(ih)

        # Skip two ATEs: one for close ATE and one for GC done ATE
        ate_size = AteZMS.size_from_write_block_size(self.write_block_size)
        self._change_sector_ate_offset(2 * ate_size)

        # Now the sector is ready for data ATEs.

    def _write_single_data_record(self, ih, record_id, data):
        if len(data) > SmallData.MAX_SIZE:
            self._write_data_record_big(ih, record_id, data)
        else:
            self._write_data_record_small(ih, record_id, data)

    # Extraction

    def _is_close_ate(self, ate):
        if not ate.is_valid_crc8():
            return False

        close_ate_cmp = CloseAteZMS(cycle_cnt=ate.cycle_cnt, offset=ate.content.offset)

        if not close_ate_cmp == ate:
            return False

        ate_size = AteZMS.size_from_write_block_size(self.write_block_size)
        valid_offset = (self.sector_size - ate.content.offset) % ate_size == 0

        return valid_offset

    def _is_empty_ate(self, ate):
        if not ate.is_valid_crc8():
            return False

        empty_ate_cmp = EmptyAteZMS(cycle_cnt=ate.cycle_cnt)

        return empty_ate_cmp == ate

    def _validate_data_within_sector(self, ate, ate_ptr, data_ptr):
        if ate.content.offset < data_ptr:
            # Invalid data offset, it cannot point to the offset in
            # the scope of the previous ATE
            return False

        if (ate.content.offset + ate.len) >= ate_ptr:
            # Invalid data len, it cannot point to the offset
            # where ATE records are already placed
            return False

        return True

    def _parse_record_data(self, ate, sector, ate_ptr, data_ptr):
        if isinstance(ate.content, SmallData):
            return True, ate.content.data[:ate.len]
        else:
            if isinstance(ate.content.additional_info, DataCRC):
                if not self._validate_data_within_sector(ate, ate_ptr, data_ptr):
                    return False, None

                data = sector[ate.content.offset:(ate.content.offset + ate.len)]
                data_ptr = ate.content.offset + ate.len

                if not ate.content.additional_info.verify_crc32(data):
                    # CRC32 mismatch, ignore the record
                    return True, None

                return True, data
            else:
                # Special ATE with Metadata, ignore the record
                return True, None

    def _parse_sector(self, sector):
        data_ptr = 0
        records = {}
        sector_status = settings_core.SectorStatus.NA

        if self._is_erased_sector(sector):
            # If erased, there is nothing else to check
            return self._prepare_sector_records_dict(settings_core.SectorStatus.ERASED, None)

        ate_size = AteZMS.size_from_write_block_size(self.write_block_size)

        # Check last ATE in the sector, which shall be the empty ATE
        ate_ptr = len(sector) - ate_size
        empty_ate = AteZMS.deserialize(sector[ate_ptr:(ate_ptr + ate_size)],
                                        self.write_block_size)

        if not self._is_empty_ate(empty_ate):
            # ZMS sector shall have the empty ATE at the end
            return self._prepare_sector_records_dict(settings_core.SectorStatus.NA, None)

        current_cycle_cnt = empty_ate.cycle_cnt

        # Check the second last ATE in the sector, which should be the close ATE if present
        ate_ptr -= ate_size
        close_ate = AteZMS.deserialize(sector[ate_ptr:(ate_ptr + ate_size)],
                                        self.write_block_size)

        if self._is_close_ate(close_ate) and (empty_ate.cycle_cnt == close_ate.cycle_cnt):
            sector_status = settings_core.SectorStatus.CLOSED
        else:
            # No close ATE yet or old one, sector should be open
            sector_status = settings_core.SectorStatus.OPEN

        while ate_ptr >= 0:
            ate_ptr -= ate_size

            if ate_ptr < data_ptr:
                # ATE would be already in the data segment of the sector,
                # ending analysis in this sector
                break

            ate = AteZMS.deserialize(sector[ate_ptr:(ate_ptr + ate_size)],
                                        self.write_block_size)

            if not ate.is_valid(current_cycle_cnt):
                # Invalid ATE, move to the next one
                continue

            valid_sector, data = self._parse_record_data(ate, sector, ate_ptr, data_ptr)

            if not valid_sector:
                return self._prepare_sector_records_dict(settings_core.SectorStatus.NA, None)

            if data is not None:
                records[ate.id] = data

        # If classified as open but there was not even one ATE record then it might be just erased
        if sector_status == settings_core.SectorStatus.OPEN and len(records) == 0:
            return self._prepare_sector_records_dict(settings_core.SectorStatus.ERASED, None)

        return self._prepare_sector_records_dict(sector_status, records)
