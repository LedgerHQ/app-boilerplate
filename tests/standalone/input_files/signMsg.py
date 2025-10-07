# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Sign Message
"""
from enum import IntEnum
from typing import List, Optional
from dataclasses import dataclass

from ragger.navigator import NavInsID

from standalone.input_files.derive_address import DeriveAddressTestCase
from application_client.app_def import AddressType, Mainnet


class MessageAddressFieldType(IntEnum):
    ADDRESS = 0x01
    KEY_HASH = 0x02

@dataclass
class MessageData:
    """CIP-8 message signing"""
    messageHex: str
    signingPath: str
    hashPayload: bool
    isAscii: bool
    addressFieldType: MessageAddressFieldType
    addressDesc: Optional[DeriveAddressTestCase] = None

@dataclass
class NavigationData:
    init: List[NavInsID]
    chunk: List[NavInsID]
    confirm: List[NavInsID]

@dataclass
class SignMsgTestCase:
    name: str
    msgData: MessageData
    nav: NavigationData


# pylint: disable=line-too-long
signMsgTestCases = [
        SignMsgTestCase("msg01: Should correctly sign an empty message with keyhash as address field",
                        MessageData("",
                                    "m/1852'/1815'/0'/0/1",
                                    False,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg02: Should correctly sign a short non-hashed ascii message with keyhash as address field",
                        MessageData("68656c6c6f20776f726c64", # "hello world"
                                    "m/1852'/1815'/0'/0/1",
                                    False,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK] * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg03: Should correctly sign a short hashed ascii message with keyhash as address field",
                        MessageData("68656c6c6f20776f726c64", # "hello world"
                                    "m/1852'/1815'/0'/0/1",
                                    True,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK] * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg04: Should correctly sign a short non-hashed ascii message displayed as hex",
                        MessageData("68656c6c6f20776f726c64", # "hello world"
                                    "m/1852'/1815'/0'/4/0",
                                    False,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 3,
                                       [NavInsID.BOTH_CLICK] * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg05: Should correctly sign a short non-hashed hex message with keyhash as address field",
                        MessageData("ff656c6c6f20776f726c64",
                                    "m/1853'/1815'/0'/0'",
                                    False,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK] * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg06: Should correctly sign a short hashed hex message with keyhash as address field",
                        MessageData("ff656c6c6f20776f726c64",
                                    "m/1853'/1815'/0'/0'",
                                    True,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK] * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg07: Should correctly sign a 198 bytes long non-hashed ascii message with keyhash as address field",
                        MessageData(f"{'6869'*99}",
                                    "m/1852'/1815'/0'/3/0",
                                    True,
                                    True,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 3,
                                       [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK],
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg08: Should correctly sign a 99 bytes long non-hashed hex message with keyhash as address field",
                        MessageData(f"{'de'*99}",
                                    "m/1852'/1815'/0'/3/0",
                                    True,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 3,
                                       [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] * 3 + [NavInsID.BOTH_CLICK],
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg09: Should correctly sign a 1000 bytes long hashed ascii message with keyhash as address field",
                        MessageData(f"{'6869'*500}",
                                    "m/1852'/1815'/0'/3/0",
                                    True,
                                    True,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 3,
                                       [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK],
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg10: Should correctly sign a 349 bytes long hashed hex message with keyhash as address field",
                        MessageData(f"{'fa'*349}",
                                    "m/1852'/1815'/0'/3/0",
                                    True,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 3,
                                       [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] * 3 + [NavInsID.BOTH_CLICK],
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg11: Should correctly sign a short non-hashed hex message with base address in address field",
                        MessageData("deadbeef",
                                    "m/1852'/1815'/0'/5/0",
                                    False,
                                    False,
                                    MessageAddressFieldType.ADDRESS,
                                    DeriveAddressTestCase("",
                                                          Mainnet,
                                                          AddressType.BASE_PAYMENT_KEY_STAKE_KEY,
                                                          "m/1852'/1815'/0'/0/1",
                                                          "m/1852'/1815'/0'/2/0")),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK]  * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg12: Should correctly sign a short non-hashed hex message with reward address in address field",
                        MessageData("deadbeef",
                                    "m/1852'/1815'/0'/5/0",
                                    False,
                                    False,
                                    MessageAddressFieldType.ADDRESS,
                                    DeriveAddressTestCase("",
                                                          Mainnet,
                                                          AddressType.REWARD_KEY,
                                                          "",
                                                          "m/1852'/1815'/0'/2/0")),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK]  * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
]
