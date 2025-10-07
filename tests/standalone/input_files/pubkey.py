# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Public Key check
"""

from dataclasses import dataclass
from typing import Optional


@dataclass
class PubKeyTestCase:
    name: str
    path: str
    nav: Optional[bool] = True


# pylint: disable=line-too-long
testsByron = [
    PubKeyTestCase("byron/path_1",
                   "m/44'/1815'/1'"),
    PubKeyTestCase("byron/path_2",
                   "m/44'/1815'/1'/0/55'"),
    PubKeyTestCase("byron/path_3",
                   "m/44'/1815'/1'/0/12'"),
]

testsShelleyUsual = [
    PubKeyTestCase("shelley_usual/path_0",
                   "m/1852'/1815'/4'"),
    PubKeyTestCase("shelley_usual/path_1",
                   "m/1852'/1815'/0'/0/1"),
    PubKeyTestCase("shelley_usual/path_2",
                   "m/1852'/1815'/0'/2/0"),
    PubKeyTestCase("shelley_usual/path_3",
                   "m/1852'/1815'/0'/2/1001"),
    PubKeyTestCase("shelley_usual/path_4",
                   "m/1852'/1815'/0'/3/0"),
    PubKeyTestCase("shelley_usual/path_5",
                   "m/1852'/1815'/0'/4/0"),
    PubKeyTestCase("shelley_usual/path_6",
                   "m/1852'/1815'/1'/5/0"),
]

testsShelleyUnusual = [
    PubKeyTestCase("shelley_unusual/path_1",
                   "m/1852'/1815'/101'"),
    PubKeyTestCase("shelley_unusual/path_2",
                   "m/1852'/1815'/100'/0/1000001'"),
    PubKeyTestCase("shelley_unusual/path_3",
                   "m/1852'/1815'/0'/2/1000001"),
    PubKeyTestCase("shelley_unusual/path_4",
                   "m/1852'/1815'/101'/3/0"),
    PubKeyTestCase("shelley_unusual/path_5",
                   "m/1852'/1815'/101'/4/0"),
    PubKeyTestCase("shelley_unusual/path_6",
                   "m/1852'/1815'/101'/5/0"),
]

testsColdKeys = [
     PubKeyTestCase("cold_case",
                    "m/1853'/1815'/0'/0'"),
]

testsCVoteKeysUsual = [
    PubKeyTestCase("CVote_keys/path_2",
                   "m/1694'/1815'/100'"),
]

testsCVoteKeysUnusual = [
    PubKeyTestCase("CVote_keys/path_1",
                   "m/1694'/1815'/0'/0/1"),
    PubKeyTestCase("CVote_keys/path_3",
                   "m/1694'/1815'/101'"),
]

rejectTestCases = [
    PubKeyTestCase("path_shorter_than_3_indexes",
                   "m/44'/1815'"),
    PubKeyTestCase("path_not_matching_cold_key_structure",
                   "m/1853'/1900'/0'/0/0"),
    PubKeyTestCase("invalid_vote_key_path_1",
                   "m/1694'/1815'/0'/1/0"),
    PubKeyTestCase("invalid_vote_key_path_2",
                   "m/1694'/1815'/17"),
    PubKeyTestCase("invalid_vote_key_path_3",
                   "m/1694'/1815'/0'/1"),
]

# TODO incomplete! missing at least
#        case PATH_DREP_KEY:
#        case PATH_COMMITTEE_COLD_KEY:
#        case PATH_COMMITTEE_HOT_KEY:
