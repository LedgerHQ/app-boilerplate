# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Derive Native Script Hash check
"""

from __future__ import annotations
from enum import IntEnum
from typing import List, Optional, Union
from dataclasses import dataclass, field

from application_client.status_words import StatusWord


class NativeScriptType(IntEnum):
    PUBKEY_DEVICE_OWNED = 0x00
    PUBKEY_THIRD_PARTY = 0xF0
    ALL = 0x01
    ANY = 0x02
    N_OF_K = 0x03
    INVALID_BEFORE = 0x04
    INVALID_HEREAFTER = 0x05

class NativeScriptHashDisplayFormat(IntEnum):
    BECH32 = 0x01
    POLICY_ID = 0x02

@dataclass
class NativeScript:
    type: NativeScriptType
    params: NativeScriptParams

@dataclass
class NativeScriptParamsPubkey:
    key: str

@dataclass
class NativeScriptParamsScripts:
    scripts: List[NativeScript] = field(default_factory=list)

@dataclass
class NativeScriptParamsNofK:
    requiredCount: int
    scripts: List[NativeScript] = field(default_factory=list)

@dataclass
class NativeScriptParamsInvalid:
    slot: int

NativeScriptParams = Union[NativeScriptParamsPubkey,
                           NativeScriptParamsScripts,
                           NativeScriptParamsNofK,
                           NativeScriptParamsInvalid]

@dataclass
class SignedData:
    hash: Optional[str] = None
    sw: Optional[StatusWord] = StatusWord.SWO_SUCCESS


@dataclass
class ValidNativeScriptTestCase:
    name: str
    script: NativeScript
    expected: SignedData
    displayFormat: Optional[NativeScriptHashDisplayFormat] = NativeScriptHashDisplayFormat.BECH32
    nano_skip: Optional[bool] = False


# pylint: disable=line-too-long
ValidNativeScriptTestCases = [
    ValidNativeScriptTestCase("PUBKEY_device_owned",
                              NativeScript(NativeScriptType.PUBKEY_DEVICE_OWNED,
                                           NativeScriptParamsPubkey("m/1852'/1815'/0'/0/0")),   
                              SignedData("e02316efa0632d53c28c521fc7bcade6e929849ff8b44efb5a2cffc0")),
    ValidNativeScriptTestCase("PUBKEY_third_party",
                              NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                           NativeScriptParamsPubkey("3a55d9f68255dfbefa1efd711f82d005fae1be2e145d616c90cf0fa9")),
                              SignedData("855228f5ecececf9c85618007cc3c2e5bdf5e6d41ef8d6fa793fe0eb")),
    ValidNativeScriptTestCase("PUBKEY_third_party_script_hash_displayed_as_policy_id",
                              NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                           NativeScriptParamsPubkey("3a55d9f68255dfbefa1efd711f82d005fae1be2e145d616c90cf0fa9")),
                              SignedData("855228f5ecececf9c85618007cc3c2e5bdf5e6d41ef8d6fa793fe0eb"),
                              NativeScriptHashDisplayFormat.POLICY_ID),
    ValidNativeScriptTestCase("ALL_script",
                              NativeScript(NativeScriptType.ALL,
                                           NativeScriptParamsScripts(
                                             [NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                           NativeScriptParamsPubkey("c4b9265645fde9536c0795adbcc5291767a0c61fd62448341d7e0386")),
                                              NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                           NativeScriptParamsPubkey("0241f2d196f52a92fbd2183d03b370c30b6960cfdeae364ffabac889"))])),
                              SignedData("af5c2ce476a6ede1c879f7b1909d6a0b96cb2081391712d4a355cef6")),
    ValidNativeScriptTestCase("ALL_script_no_subscripts",
                              NativeScript(NativeScriptType.ALL,
                                           NativeScriptParamsScripts()),
                              SignedData("d441227553a0f1a965fee7d60a0f724b368dd1bddbc208730fccebcf")),
    ValidNativeScriptTestCase("ANY_script",
                              NativeScript(NativeScriptType.ANY,
                                           NativeScriptParamsScripts(
                                             [NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                           NativeScriptParamsPubkey("c4b9265645fde9536c0795adbcc5291767a0c61fd62448341d7e0386")),
                                              NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                           NativeScriptParamsPubkey("0241f2d196f52a92fbd2183d03b370c30b6960cfdeae364ffabac889"))])),
                              SignedData("d6428ec36719146b7b5fb3a2d5322ce702d32762b8c7eeeb797a20db")),
    ValidNativeScriptTestCase("ANY_script_no_subscripts",
                              NativeScript(NativeScriptType.ANY,
                                           NativeScriptParamsScripts()),
                              SignedData("52dc3d43b6d2465e96109ce75ab61abe5e9c1d8a3c9ce6ff8a3af528")),
    ValidNativeScriptTestCase("N_OF_K_script",
                              NativeScript(NativeScriptType.N_OF_K,
                                           NativeScriptParamsNofK(2,
                                             [NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                           NativeScriptParamsPubkey("c4b9265645fde9536c0795adbcc5291767a0c61fd62448341d7e0386")),
                                              NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                           NativeScriptParamsPubkey("0241f2d196f52a92fbd2183d03b370c30b6960cfdeae364ffabac889"))])),
                              SignedData("78963f8baf8e6c99ed03e59763b24cf560bf12934ec3793eba83377b")),
    ValidNativeScriptTestCase("N_OF_K_script_no_subscripts",
                              NativeScript(NativeScriptType.N_OF_K,
                                           NativeScriptParamsNofK(0)),
                              SignedData("3530cc9ae7f2895111a99b7a02184dd7c0cea7424f1632d73951b1d7")),
    ValidNativeScriptTestCase("INVALID_BEFORE_script",
                              NativeScript(NativeScriptType.INVALID_BEFORE,
                                           NativeScriptParamsInvalid(42)),
                              SignedData("2a25e608a683057e32ea38b50ce8875d5b34496b393da8d25d314c4e")),
    ValidNativeScriptTestCase("INVALID_BEFORE_script_slot_is_a_big_number",
                              NativeScript(NativeScriptType.INVALID_BEFORE,
                                           NativeScriptParamsInvalid(18446744073709551615)),
                              SignedData("d2469adac494849dd27d1b344b74cc6cd5bf31fbd01c879eae84c04b")),
    ValidNativeScriptTestCase("INVALID_HEREAFTER_script",
                              NativeScript(NativeScriptType.INVALID_HEREAFTER,
                                           NativeScriptParamsInvalid(42)),
                              SignedData("1620dc65993296335183f23ff2f7747268168fabbeecbf24c8a20194")),
    ValidNativeScriptTestCase("INVALID_HEREAFTER_script_slot_is_a_big_number",
                              NativeScript(NativeScriptType.INVALID_HEREAFTER,
                                           NativeScriptParamsInvalid(18446744073709551615)),
                              SignedData("da60fa40290f93b889a88750eb141fd2275e67a1255efb9bac251005")),
    ValidNativeScriptTestCase("Nested_native_scripts",
                              NativeScript(NativeScriptType.ALL,
                                           NativeScriptParamsScripts(
                                             [NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                           NativeScriptParamsPubkey("c4b9265645fde9536c0795adbcc5291767a0c61fd62448341d7e0386")),
                                              NativeScript(NativeScriptType.ANY,
                                                           NativeScriptParamsScripts(
                                                               [NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                                             NativeScriptParamsPubkey("c4b9265645fde9536c0795adbcc5291767a0c61fd62448341d7e0386")),
                                                                NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                                             NativeScriptParamsPubkey("0241f2d196f52a92fbd2183d03b370c30b6960cfdeae364ffabac889"))])),
                                             NativeScript(NativeScriptType.N_OF_K,
                                                          NativeScriptParamsNofK(2,
                                                           [NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                                        NativeScriptParamsPubkey("c4b9265645fde9536c0795adbcc5291767a0c61fd62448341d7e0386")),
                                                            NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                                         NativeScriptParamsPubkey("0241f2d196f52a92fbd2183d03b370c30b6960cfdeae364ffabac889")),
                                                            NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                                         NativeScriptParamsPubkey("cecb1d427c4ae436d28cc0f8ae9bb37501a5b77bcc64cd1693e9ae20"))])),
                                             NativeScript(NativeScriptType.INVALID_BEFORE, NativeScriptParamsInvalid(100)),
                                             NativeScript(NativeScriptType.INVALID_HEREAFTER, NativeScriptParamsInvalid(200))])),
                              SignedData("0d63e8d2c5a00cbcffbdf9112487c443466e1ea7d8c834df5ac5c425"),
                              nano_skip=True),
    ValidNativeScriptTestCase("Nested native scripts #2",
                              NativeScript(NativeScriptType.ALL,
                                           NativeScriptParamsScripts(
                                             [NativeScript(NativeScriptType.ANY,
                                                           NativeScriptParamsScripts(
                                                               [NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                                             NativeScriptParamsPubkey("c4b9265645fde9536c0795adbcc5291767a0c61fd62448341d7e0386")),
                                                                NativeScript(NativeScriptType.PUBKEY_THIRD_PARTY,
                                                                             NativeScriptParamsPubkey("0241f2d196f52a92fbd2183d03b370c30b6960cfdeae364ffabac889"))]))])),
                              SignedData("903e52ef2421abb11562329130330763583bb87cd98006b70ecb1b1c"),
                              nano_skip=True),
    ValidNativeScriptTestCase("Nested native scripts #3",
                              NativeScript(NativeScriptType.N_OF_K,
                                           NativeScriptParamsNofK(0,
                                             [NativeScript(NativeScriptType.ALL,
                                                           NativeScriptParamsScripts(
                                                               [NativeScript(NativeScriptType.ANY,
                                                                             NativeScriptParamsScripts(
                                                                                 [NativeScript(NativeScriptType.N_OF_K,
                                                                                              NativeScriptParamsNofK(0))]))]))])),
                              SignedData("ed1dd7ef95caf389669c62618eb7f7aa7eadd08feb76618db2ae0cfc"),
                              nano_skip=True),
]