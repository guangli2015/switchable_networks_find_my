#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import base64
import argparse
import sys
import io

import intelhex

from . import device as DEVICE
from . import provisioned_metadata as PROVISIONED_METADATA

from .device_type import DeviceType

from . import nv_storage_type as storage_type
from .settings import creator as settings_creator

from .tool import creator as tool_creator

def extract_error_handle(msg, param_prefix = None):
    parser.print_usage()

    if param_prefix is None:
        param_prefix = ''
    else:
        param_prefix = 'argument %s: ' % param_prefix
    print('extract: error: ' + param_prefix + msg)

    sys.exit(1)

def sn_choose(serial_numbers, snr=None):
    if not snr:
        if not serial_numbers:
            extract_error_handle('no devices connected')
        elif len(serial_numbers) == 1:
            snr = serial_numbers[0]
        else:
            # User input required
            serial_numbers = {idx+1: serial_number for idx, serial_number in enumerate(serial_numbers)}
            print('Choose the device:')
            for idx, sn in serial_numbers.items():
                print(f"{idx}. {sn}")
            decision = input()

            try:
                decision = int(decision)
            except ValueError:
                choice_list = ', '.join(str(key) for key in  serial_numbers.keys())
                extract_error_handle('invalid choice (choose from: %s)' % choice_list)

            if decision in serial_numbers.keys():
                snr = serial_numbers[decision]
            elif decision in serial_numbers.values():
                # Option to provide serial number instead of index, for automation purpose.
                # Usage: "echo 123456789 | ncsfmntools extract -e NRF52840"
                snr = decision
            else:
                choice_list = ', '.join(str(key) for key in  serial_numbers.keys())
                extract_error_handle('invalid choice (choose from: %s)' % choice_list)
    elif snr not in serial_numbers:
        extract_error_handle(f'device with serial number: {snr} cannot be found')

    return snr

def remove_zeros(items):
    while items and len(items) > 0 and items[-1] == 0:
        items.pop()

def hex_arg_to_int(hex_str):
    try:
        return int(hex_str, 16)
    except ValueError:
        return None

def cli(cmd, argv):
    global parser

    parser = argparse.ArgumentParser(description='FMN Accessory MFi Token Extractor Tool', prog=cmd, add_help=False)
    parser.add_argument('-e', '--device', default=None, help='Device of accessory to use',
              choices=[dev.value for dev in DEVICE.DEVICE_DESC], required=True)
    parser.add_argument('-i', '--input-file', default=None, help='File in *.hex or *.bin format with Settings partition memory dump')
    parser.add_argument('-f', '--settings-base', default=None, metavar='ADDRESS',
              help='Settings base address given in hex format. This only needs to be specified if the default values in the '
                   'NCS has been changed.')
    parser.add_argument('-s', '--settings-size', default=None, metavar='SIZE',
              help='Settings partition size given in hex format. This only needs to be specified if the default values in the '
                    'NCS has been changed.')
    parser.add_argument('-n', '--nv-storage', help='Non-volatile storage type', choices=[t.value for t in storage_type.NVStorageType])
    parser.add_argument('--help', action='help',
                        help='Show this help message and exit')

    args = parser.parse_args(argv)

    if (args.settings_size is not None) and (args.settings_base is None):
        extract_error_handle("argument -s/--settings-size: requires -f/--settings-base argument")

    if args.input_file is not None:
        if (args.settings_base is not None) or (args.settings_size is not None):
            extract_error_handle("argument -f/--settings-base and -s/--setings-size: cannot be used with -i/--input-file argument")

    if args.input_file:
        bin_str = load_from_file(args.input_file)
    else:
        bin_str = load_from_device(args.device, args.settings_base, args.settings_size)

    extract(bin_str, args.device, args.settings_base, args.settings_size, args.nv_storage)

def settings_partition_input_handle(settings_base, settings_size, device):
    base_param_prefix = '-f/--settings-base'
    size_param_prefix = '-s/--settings-size'

    nvm_base = DEVICE.DEVICE_DESC[device]['nvm']['base_address']
    nvm_size = DEVICE.DEVICE_DESC[device]['nvm']['size']
    nvm_end = nvm_base + nvm_size

    if (settings_base is not None) and (settings_size is not None):
        settings_base = hex_arg_to_int(settings_base)
        settings_size = hex_arg_to_int(settings_size)

        if settings_base is None:
            extract_error_handle('malformed memory address', base_param_prefix)
        if settings_size is None:
            extract_error_handle('malformed memory size', size_param_prefix)
    elif (settings_base is not None) and (settings_size is None):
        # If only settings base is provided, sweep the memory from the settings base until the end of the NVM
        # to ensure that the legacy behavior is kept.
        settings_base = hex_arg_to_int(settings_base)

        if settings_base is None:
            extract_error_handle('malformed memory address', base_param_prefix)

        settings_size = nvm_end - settings_base
    else:
        # If neither settings base nor size is provided, use the default settings partition
        settings_base = nvm_base + DEVICE.DEVICE_DESC[device]['default_settings_partition']['relative_address']
        settings_size = DEVICE.DEVICE_DESC[device]['default_settings_partition']['size']

    if settings_size == 0:
        extract_error_handle('settings partition size cannot be 0', size_param_prefix)

    settings_end = settings_base + settings_size

    if settings_base < nvm_base:
        extract_error_handle('settings partition is placed before the target device memory: %s < %s'
            % (hex(settings_base), hex(nvm_base)), base_param_prefix)

    if nvm_end < settings_end:
        extract_error_handle('settings partition is bigger than the target device memory: %s >= %s'
            % (hex(settings_end), hex(nvm_end)), size_param_prefix)

    if settings_base % DEVICE.SETTINGS_SECTOR_SIZE != 0:
        aligned_page = hex(settings_base & ~(DEVICE.SETTINGS_SECTOR_SIZE - 1))
        extract_error_handle('address should be page aligned: %s -> %s'
              % (hex(settings_base), aligned_page), base_param_prefix)

    if settings_size % DEVICE.SETTINGS_SECTOR_SIZE != 0:
        aligned_page = hex(settings_size & ~(DEVICE.SETTINGS_SECTOR_SIZE - 1))
        extract_error_handle('size should be page aligned: %s -> %s'
              % (hex(settings_size), aligned_page), size_param_prefix)

    return settings_base, settings_size

def nv_storage_input_handle(nv_storage, device):
    if nv_storage:
        return storage_type.NVStorageType(nv_storage)

    return DEVICE.DEVICE_DESC[device]['default_settings_partition']['storage_type']

def load_from_device(device, settings_base, settings_size):
    settings_base, settings_size = settings_partition_input_handle(settings_base, settings_size, device)
    print('Looking for the provisioned data in the following memory range: %s - %s'
        % (hex(settings_base), hex(settings_base + settings_size)))
    selected_tool = DEVICE.DEVICE_DESC[device]['tool']

    tool = tool_creator.tool_instance_create(selected_tool)

    snrs = tool.snr_list_get()
    snr = sn_choose(snrs, None)
    bin_data = tool.memory_read(snr, settings_base, settings_size)

    return bytes(bin_data)

def load_from_file(filename):
    print("Searching for the provisioned data in provided settings partition memory dump file")

    # Read content from assumed settings partition memory dump
    bin_str = b''
    if filename.endswith(".hex"):
        out = io.BytesIO(b"")
        intelhex.hex2bin(filename, out)
        bin_str = out.getvalue()
    elif filename.endswith(".bin"):
        with open(filename, "rb") as f:
            bin_str = f.read()
    else:
        extract_error_handle("Not supported file type, use .hex or .bin!")

    # Align input to settings sector size
    bin_str = bin_str.ljust(DEVICE.SETTINGS_SECTOR_SIZE, DEVICE.NVM_ERASE_VALUE)

    return bin_str

def extract(bin_str, device, settings_base, settings_size, nv_storage):
    nv_storage = nv_storage_input_handle(nv_storage, device)
    settings_base, settings_size = settings_partition_input_handle(settings_base, settings_size, device)

    settings_sector_size = DEVICE.SETTINGS_SECTOR_SIZE
    device_write_block_size = DEVICE.DEVICE_DESC[device]['nvm']['write_block_size']
    device_nvm_erase_value = DEVICE.NVM_ERASE_VALUE

    print(f'''Using:
    * Device: {device}
    * Storage system: {nv_storage}
    * Settings base: {hex(settings_base)}
    * Settings size: {hex(settings_size)}
    * Settings sector size: {settings_sector_size} bytes
    * Device write block size: {device_write_block_size} bytes''')

    settings = settings_creator.settings_instance_create(nv_storage, settings_base,
                                                         settings_sector_size,
                                                         device_write_block_size,
                                                         device_nvm_erase_value)

    kv_records = settings.read(bin_str)

    if (kv_records is None) or (len(kv_records) == 0):
        extract_error_handle("Provided settings partition does not contain any records")

    # Get the UUID Value
    auth_uuid = kv_records.get(PROVISIONED_METADATA.MFI_TOKEN_UUID.KEY)

    if auth_uuid is not None:
        auth_uuid = auth_uuid.hex()
        print("SW Authentication UUID: %s-%s-%s-%s-%s" % (
            auth_uuid[:8],
            auth_uuid[8:12],
            auth_uuid[12:16],
            auth_uuid[16:20],
            auth_uuid[20:]))
    else:
        print("SW Authentication UUID: not found in the provisioned data")

    # Get the Authentication Token Value
    auth_token = kv_records.get(PROVISIONED_METADATA.MFI_AUTH_TOKEN.KEY)

    if auth_token is not None:
        # Trim zeroes at the end and covert to base64 format
        auth_token = bytearray(auth_token)
        remove_zeros(auth_token)
        auth_token_base64 = base64.b64encode(auth_token).decode()

        print("SW Authentication Token: %s" % auth_token_base64)
    else:
        print("SW Authentication Token: not found in the provisioned data")

    # Get the Serial Number Value (optional)
    serial_number = kv_records.get(PROVISIONED_METADATA.SERIAL_NUMBER.KEY)

    if serial_number:
        print("Serial Number: %s" % serial_number.hex().upper())
    else:
        print("Serial Number: not found in the provisioned data")

    # Extracting operation was not successful, exit with error
    if (auth_uuid is None) or (auth_token is None):
        extract_error_handle("Provisioned data does not contain valid MFi token")

if __name__ == '__main__':
    cli()
