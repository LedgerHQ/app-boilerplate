# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
Ensures the Python test client defines the same command constants as the C app.
"""

from pathlib import Path
import re

from application_client.command_builder import CLA, InsType, P1Type, P2Type
from standalone.input_files.signTx import MAX_SIGN_TX_CHUNK_SIZE


def _parse_defines(path: Path) -> dict[str, int]:
    defines = {}
    for line in path.read_text().splitlines():
        match = re.match(r"#define\s+(\w+)\s+(0x[0-9A-Fa-f]+|\d+)", line)
        if match:
            defines[match.group(1)] = int(match.group(2), 0)
    return defines


def _parse_enum(path: Path) -> dict[str, int]:
    values = {}
    for line in path.read_text().splitlines():
        for match in re.finditer(r"(\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", line):
            values[match.group(1)] = int(match.group(2), 0)
    return values


def test_ins_constants_match_src():
    root = Path(__file__).resolve().parents[2]
    ins_defines = _parse_enum(root / "src" / "apdu" / "apdu_constants.h")
    expected = {
        "INS_GET_SERIAL": InsType.INS_GET_SERIAL,
        "INS_GET_VERSION": InsType.INS_GET_VERSION,
        "INS_GET_APP_NAME": InsType.INS_GET_APP_NAME,
        "INS_GET_PUBLIC_KEY": InsType.INS_GET_PUBLIC_KEY,
        "INS_SIGN_TX": InsType.INS_SIGN_TX,
        "INS_SIGN_OPCERT": InsType.INS_SIGN_OPCERT,
    }
    for name, value in expected.items():
        assert ins_defines.get(name) == int(value), f"{name} mismatch"


def test_p1_p2_constants_match_src():
    root = Path(__file__).resolve().parents[2]
    dispatcher_path = root / "src" / "apdu" / "dispatcher.h"
    dispatcher_defines = _parse_defines(dispatcher_path)
    dispatcher_enum = _parse_enum(dispatcher_path)
    assert dispatcher_defines["P1_TX_INIT"] == P1Type.P1_TX_INIT
    assert dispatcher_defines["P1_TX_DATA_CHUNK"] == P1Type.P1_TX_DATA_CHUNK
    assert dispatcher_defines["P1_TX_CHUNK_LAST"] == P1Type.P1_TX_CHUNK_LAST
    assert dispatcher_enum["P1_UNUSED"] == 0
    assert dispatcher_enum["P2_UNUSED"] == 0
    assert P2Type.P2_UNUSED == 0


def test_cla_constant_match_src():
    root = Path(__file__).resolve().parents[2]
    defines = _parse_defines(root / "src" / "apdu" / "apdu_constants.h")
    assert defines["CLA"] == CLA


def test_max_sign_tx_chunk_size():
    root = Path(__file__).resolve().parents[2]
    defines = _parse_defines(root / "src" / "handler" / "sign_tx.h")
    assert defines["MAX_SIGN_TX_CHUNK_SIZE"] == MAX_SIGN_TX_CHUNK_SIZE
