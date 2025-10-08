# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests Client application.
It contains the application definitions.
"""
from enum import IntEnum
from dataclasses import dataclass
from typing import Union


class Errors(IntEnum):
    """Application Errors definitions"""

    SW_MALFORMED_REQUEST_HEADER   = 0x6E01
    SW_BAD_CLA                    = 0x6E02
    SW_UNKNOWN_INS                = 0x6E03
    SW_STILL_IN_CALL              = 0x6E04
    SW_INVALID_REQUEST_PARAMETERS = 0x6E05
    SW_INVALID_STATE              = 0x6E06
    SW_INVALID_DATA               = 0x6E07
    SW_REJECTED_BY_USER           = 0x6E09
    SW_REJECTED_BY_POLICY         = 0x6E10
    SW_DEVICE_LOCKED              = 0x6E11
    SW_SWAP_CHECKING_FAIL         = 0x6E13
    SW_SUCCESS                    = 0x9000


class ProtocolMagics(IntEnum):
    MAINNET = 0x2D964A09        # 764824073
    TESTNET = 0x2A              # 42, For integration tests
    TESTNET_LEGACY = 0x4170CB17 # 1097911063
    TESTNET_PREPROD = 1
    TESTNET_PREVIEW = 2
    FAKE = 47


class NetworkIds(IntEnum):
    TESTNET = 0x00
    MAINNET = 0x01
    FAKE = 0x03


class AddressType(IntEnum):
    BASE_PAYMENT_KEY_STAKE_KEY = 0x00
    BASE_PAYMENT_SCRIPT_STAKE_KEY = 0x01
    BASE_PAYMENT_KEY_STAKE_SCRIPT = 0x02
    BASE_PAYMENT_SCRIPT_STAKE_SCRIPT = 0x03
    POINTER_KEY = 0x04
    POINTER_SCRIPT = 0x05
    ENTERPRISE_KEY = 0x06
    ENTERPRISE_SCRIPT = 0x07
    BYRON = 0x08
    REWARD_KEY = 0x0E
    REWARD_SCRIPT = 0x0F


class StakingDataSourceType(IntEnum):
    NONE = 0x11
    KEY_PATH = 0x22
    KEY_HASH = 0x33
    BLOCKCHAIN_POINTER = 0x44
    SCRIPT_HASH = 0x55


@dataclass
class NetworkDesc:
    networkId: Union[NetworkIds, int]
    protocol: Union[ProtocolMagics, int]


Mainnet = NetworkDesc(NetworkIds.MAINNET, ProtocolMagics.MAINNET)
Testnet = NetworkDesc(NetworkIds.TESTNET, ProtocolMagics.TESTNET)
Testnet_legacy = NetworkDesc(NetworkIds.TESTNET, ProtocolMagics.TESTNET_LEGACY)
FakeNet = NetworkDesc(NetworkIds.FAKE, ProtocolMagics.FAKE)
