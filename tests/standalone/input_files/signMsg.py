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
        SignMsgTestCase("msg01_Should_correctly_sign_an_empty_message_with_keyhash_as_address_field",
                        MessageData("",
                                    "m/1852'/1815'/0'/0/1",
                                    False,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg02_Should_correctly_sign_a_short_nonhashed_ascii_message_with_keyhash_as_address_field",
                        MessageData("68656c6c6f20776f726c64", # "hello world"
                                    "m/1852'/1815'/0'/0/1",
                                    False,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK] * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg03_Should_correctly_sign_a_short_hashed_ascii_message_with_keyhash_as_address_field",
                        MessageData("68656c6c6f20776f726c64", # "hello world"
                                    "m/1852'/1815'/0'/0/1",
                                    True,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK] * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg04_Should_correctly_sign_a_short_nonhashed_ascii_message_displayed_as_hex",
                        MessageData("68656c6c6f20776f726c64", # "hello world"
                                    "m/1852'/1815'/0'/4/0",
                                    False,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 3,
                                       [NavInsID.BOTH_CLICK] * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg05_Should_correctly_sign_a_short_nonhashed_hex_message_with_keyhash_as_address_field",
                        MessageData("ff656c6c6f20776f726c64",
                                    "m/1853'/1815'/0'/0'",
                                    False,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK] * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg06_Should_correctly_sign_a_short_hashed_hex_message_with_keyhash_as_address_field",
                        MessageData("ff656c6c6f20776f726c64",
                                    "m/1853'/1815'/0'/0'",
                                    True,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 2 + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK],
                                       [NavInsID.BOTH_CLICK] * 2,
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg07_Should_correctly_sign_a_198_bytes_long_nonhashed_ascii_message_with_keyhash_as_address_field",
                        MessageData(f"{'6869'*99}",
                                    "m/1852'/1815'/0'/3/0",
                                    True,
                                    True,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 3,
                                       [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK],
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg08_Should_correctly_sign_a_99_bytes_long_nonhashed_hex_message_with_keyhash_as_address_field",
                        MessageData(f"{'de'*99}",
                                    "m/1852'/1815'/0'/3/0",
                                    True,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 3,
                                       [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] * 3 + [NavInsID.BOTH_CLICK],
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg09_Should_correctly_sign_a_1000_bytes_long_hashed_ascii_message_with_keyhash_as_address_field",
                        MessageData(f"{'6869'*500}",
                                    "m/1852'/1815'/0'/3/0",
                                    True,
                                    True,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 3,
                                       [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK],
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg10_Should_correctly_sign_a_349_bytes_long_hashed_hex_message_with_keyhash_as_address_field",
                        MessageData(f"{'fa'*349}",
                                    "m/1852'/1815'/0'/3/0",
                                    True,
                                    False,
                                    MessageAddressFieldType.KEY_HASH),
                        NavigationData([NavInsID.BOTH_CLICK] * 3,
                                       [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] * 3 + [NavInsID.BOTH_CLICK],
                                       [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2)),
        SignMsgTestCase("msg11_Should_correctly_sign_a_short_nonhashed_hex_message_with_base_address_in_address_field",
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
        SignMsgTestCase("msg12_Should_correctly_sign_a_short_nonhashed_hex_message_with_reward_address_in_address_field",
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
