#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import collections

PROVISIONED_METADATA_KEY_FORMATTER = "fmna/provisioning/%3s"

# KEY is key string of the provisioned key-value pair in the Settings file system.
# LEN is value length in bytes of the provisioned key-value pair
ProvisionedMetadata = collections.namedtuple('ProvisionedMetadata', 'KEY LEN')

SERIAL_NUMBER = ProvisionedMetadata((PROVISIONED_METADATA_KEY_FORMATTER % 997), 16)
MFI_TOKEN_UUID = ProvisionedMetadata((PROVISIONED_METADATA_KEY_FORMATTER % 998), 16)
MFI_AUTH_TOKEN = ProvisionedMetadata((PROVISIONED_METADATA_KEY_FORMATTER % 999), 1024)
