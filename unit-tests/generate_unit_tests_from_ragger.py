#!/usr/bin/env python3
"""
Unified generator for unit-test fixtures derived from ragger/LedgerJS sources.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import types
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple, Type


UNIT_TESTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = UNIT_TESTS_DIR.parent

NODE_VERSION = "16.20.2"


def _add_tests_to_sys_path() -> None:
    sys.path.insert(0, str(REPO_ROOT / "tests"))
    sys.path.insert(0, str(REPO_ROOT / "tests" / "application_client"))
    sys.path.insert(0, str(REPO_ROOT / "tests" / "standalone"))


ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
ALPHABET_INDEX = {char: index for index, char in enumerate(ALPHABET)}


def _ensure_base58_module() -> None:
    if "base58" in sys.modules:
        return

    def b58encode(data: bytes) -> bytes:
        if not data:
            return b""

        zero_prefix = 0
        for byte in data:
            if byte == 0:
                zero_prefix += 1
            else:
                break
        num = int.from_bytes(data, "big")
        encoded_bytes = bytearray()
        while num > 0:
            num, remainder = divmod(num, 58)
            encoded_bytes.append(ord(ALPHABET[remainder]))
        encoded_bytes.reverse()
        result = bytearray(b"1" * zero_prefix)
        if encoded_bytes:
            result.extend(encoded_bytes)
        elif zero_prefix == 0:
            result.extend(b"1")
        return bytes(result)

    def b58decode(value: bytes | str) -> bytes:
        if isinstance(value, bytes):
            value = value.decode("ascii")
        if value == "":
            return b""
        num = 0
        for char in value:
            num = num * 58 + ALPHABET_INDEX[char]
        decoded = num.to_bytes((num.bit_length() + 7) // 8, "big") if num > 0 else b""
        zero_prefix = len(value) - len(value.lstrip("1"))
        return b"\x00" * zero_prefix + decoded

    module = types.ModuleType("base58")
    module.b58encode = b58encode
    module.b58decode = b58decode
    sys.modules["base58"] = module


def _load_sign_tx_tests() -> Dict[str, Any]:
    _ensure_base58_module()
    _add_tests_to_sys_path()
    from standalone.input_files.signTx import (  # type: ignore
        testsMary,
        testsShelleyNoCertificates,
        testsShelleyWithCertificates,
        testsAllegra,
        testsByron,
        testsAlonzo,
        testsAlonzoTrezorComparison,
        testsBabbage,
        testsBabbageTrezorComparison,
        testsConwayWithCertificates,
        testsConwayWithoutCertificates,
        testsConwayVotingProcedures,
        testsMultidelegation,
        testsCatalystRegistration,
        testsCVoteRegistrationCIP36,
        testsMultisig,
        poolRegistrationOwnerTestCases,
        poolRegistrationOperatorTestCases,
        TxAuxiliaryDataCIP36,
        TxAuxiliaryDataType,
        TxAuxiliaryDataHash,
    )

    era_tests = {
        "byron": testsByron,
        "shelley": testsShelleyNoCertificates,
        "shelley_certificates": testsShelleyWithCertificates,
        "allegra": testsAllegra,
        "mary": testsMary,
        "alonzo": testsAlonzo + testsAlonzoTrezorComparison + testsMultidelegation,
        "babbage": testsBabbage + testsBabbageTrezorComparison,
        "conway": testsConwayWithCertificates,
        "conway_without_certificates": testsConwayWithoutCertificates,
        "conway_voting": testsConwayVotingProcedures,
        "multisig": testsMultisig,
        "alonzo_catalyst": testsCatalystRegistration,
        "alonzo_cip36": testsCVoteRegistrationCIP36,
        "pool_registration": poolRegistrationOwnerTestCases + poolRegistrationOperatorTestCases,
    }

    return {
        "era_tests": era_tests,
        "TxAuxiliaryDataCIP36": TxAuxiliaryDataCIP36,
        "TxAuxiliaryDataType": TxAuxiliaryDataType,
        "TxAuxiliaryDataHash": TxAuxiliaryDataHash,
    }


def _format_bytes_as_c_array(data: bytes, name: str, bytes_per_line: int = 16) -> str:
    lines = []
    for i in range(0, len(data), bytes_per_line):
        chunk = data[i:i + bytes_per_line]
        hex_bytes = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_bytes},")

    result = f"static const uint8_t {name}[] = {{\n"
    result += "\n".join(lines)
    result += "\n};"
    return result


def _extract_apdu_payload(apdu: bytes) -> bytes:
    if len(apdu) < 5:
        raise ValueError("APDU too short")
    lc = apdu[4]
    payload = apdu[5:5 + lc]
    if len(payload) != lc:
        raise ValueError("APDU payload length mismatch")
    return payload


def _split_hex_string(hex_str: str, chunk_size: int = 1024) -> List[str]:
    return [hex_str[i:i + chunk_size] for i in range(0, len(hex_str), chunk_size)]


def _compute_blake2b_256(data: bytes) -> str:
    return hashlib.blake2b(data, digest_size=32).hexdigest()


def _sanitize_name_for_c(name: str) -> str:
    safe = "".join(c if c.isalnum() else "_" for c in name)
    while "__" in safe:
        safe = safe.replace("__", "_")
    return safe.upper()


def _bool_to_c(value: bool) -> str:
    return "true" if value else "false"


def _cbor_hex_to_bytes(hex_str: str) -> bytes:
    return bytes.fromhex(hex_str.replace(" ", "").replace("\n", ""))


def _extract_aux_data_hash_from_tx_body(hex_str: str) -> Optional[str]:
    import cbor2  # type: ignore

    try:
        parsed = cbor2.loads(_cbor_hex_to_bytes(hex_str))
    except Exception:
        return None

    if not isinstance(parsed, dict):
        return None

    aux_hash = parsed.get(7)
    if isinstance(aux_hash, (bytes, bytearray)) and len(aux_hash) == 32:
        return aux_hash.hex()
    return None


def _count_fixture_structs(header_text: str) -> int:
    pattern = re.compile(r"static const tx_fixture_t [A-Z0-9_]+\s*=\s*\{")
    return len(pattern.findall(header_text))


def _generate_fixtures_for_era(
    era_key: str,
    tests: Sequence[Any],
    aux_data_classes: Dict[str, Any],
) -> None:
    from application_client.command_builder import CommandBuilder, gather_witness_paths  # type: ignore

    TxAuxiliaryDataCIP36 = aux_data_classes["TxAuxiliaryDataCIP36"]
    TxAuxiliaryDataType = aux_data_classes["TxAuxiliaryDataType"]
    TxAuxiliaryDataHash = aux_data_classes["TxAuxiliaryDataHash"]

    print(f"Generating C fixtures for {era_key.upper()} era ({len(tests)} tests)...")
    print()

    header_lines = [
        f"// Auto-generated fixtures for {era_key.upper()} era transaction tests",
        "// Generated from LedgerJS signTx.ts test cases",
        "//",
        f"// Total tests: {len(tests)}",
        "",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "#include <stddef.h>",
        '#include "test_fixture_types.h"',
        "",
        "// ======================================================================",
        "// Fixtures",
        "// ======================================================================",
        "",
        "#if defined(__clang__)",
        "#pragma clang diagnostic push",
        '#pragma clang diagnostic ignored "-Woverlength-strings"',
        "#elif defined(__GNUC__)",
        "#pragma GCC diagnostic push",
        '#pragma GCC diagnostic ignored "-Woverlength-strings"',
        "#endif",
        "",
    ]

    for test_index, test_case in enumerate(tests):
        print(f"  [{test_index + 1}/{len(tests)}] {test_case.name}...")

        tx = test_case.tx
        builder = CommandBuilder()
        raw_tx_bytes = builder._serialize_transaction_unpacked_raw(tx)

        expected_cbor_hex = test_case.txBody
        cbor_bytes = _cbor_hex_to_bytes(expected_cbor_hex)
        expected_hash_hex = _compute_blake2b_256(cbor_bytes)
        body_aux_data_hash = _extract_aux_data_hash_from_tx_body(expected_cbor_hex)

        safe_name = _sanitize_name_for_c(test_case.name)
        fixture_prefix = f"FIXTURE_{era_key.upper()}_{safe_name}"

        include_aux_data_hash = tx.auxiliaryData is not None
        aux_data_type = 0
        aux_data_hash_hex = body_aux_data_hash
        aux_data_init_payload = b""
        aux_data_delegation_payloads: List[bytes] = []
        if include_aux_data_hash:
            if tx.auxiliaryData.type == TxAuxiliaryDataType.ARBITRARY_HASH:
                aux_data_type = int(TxAuxiliaryDataType.ARBITRARY_HASH)
                aux_params = tx.auxiliaryData.params
                if isinstance(aux_params, TxAuxiliaryDataHash):
                    aux_data_hash_hex = aux_params.hashHex
                else:
                    include_aux_data_hash = False
            elif tx.auxiliaryData.type == TxAuxiliaryDataType.CIP36_REGISTRATION:
                aux_data_type = int(TxAuxiliaryDataType.CIP36_REGISTRATION)
                aux_params = tx.auxiliaryData.params
                if isinstance(aux_params, TxAuxiliaryDataCIP36):
                    aux_data_init_apdu = builder.sign_tx_aux_data_init(tx, aux_params)
                    aux_data_init_payload = _extract_apdu_payload(aux_data_init_apdu)
                    for delegation in aux_params.delegations:
                        reg_apdu = builder.sign_tx_aux_data_delegation(delegation)
                        aux_data_delegation_payloads.append(_extract_apdu_payload(reg_apdu))
                else:
                    include_aux_data_hash = False

        include_script_data_hash = getattr(tx, "scriptDataHash", None) is not None

        options_value = "TX_OPTIONS_TAG_CBOR_SETS" if "d90102" in expected_cbor_hex.lower() else "0"
        network_id_value = int(test_case.tx.network.networkId)
        protocol_magic_value = int(test_case.tx.network.protocol)

        header_lines.append(f"// Test {test_index}: {test_case.name}")
        header_lines.append("//")

        array_lines = _format_bytes_as_c_array(
            raw_tx_bytes,
            f"{fixture_prefix}_RAW_TX",
        ).split("\n")
        header_lines.extend(array_lines)
        header_lines.append("")
        if include_aux_data_hash and aux_data_type == int(TxAuxiliaryDataType.CIP36_REGISTRATION):
            init_payload_name = f"{fixture_prefix}_AUX_DATA_INIT_PAYLOAD"
            init_payload_lines = _format_bytes_as_c_array(
                aux_data_init_payload,
                init_payload_name,
            ).split("\n")
            header_lines.extend(init_payload_lines)
            header_lines.append("")

            delegations_name = f"{fixture_prefix}_AUX_DATA_DELEGATIONS"
            if aux_data_delegation_payloads:
                delegation_entries = []
                for reg_index, payload in enumerate(aux_data_delegation_payloads):
                    entry_name = f"{fixture_prefix}_AUX_DATA_DELEGATION_{reg_index}"
                    reg_lines = _format_bytes_as_c_array(payload, entry_name).split("\n")
                    header_lines.extend(reg_lines)
                    header_lines.append("")
                    delegation_entries.append(
                        f"    {{ .payload = {entry_name}, .payload_len = sizeof({entry_name}) }},"
                    )
                header_lines.append(f"static const aux_data_payload_t {delegations_name}[] = {{")
                header_lines.extend(delegation_entries)
                header_lines.append("};")
            header_lines.append("")
        header_lines.append(f"static const tx_fixture_t {fixture_prefix} = {{")
        header_lines.append(f'    .name = "{test_case.name}",')
        header_lines.append(f"    .raw_tx = {fixture_prefix}_RAW_TX,")
        header_lines.append(f"    .raw_tx_len = sizeof({fixture_prefix}_RAW_TX),")
        tx_body_chunks = _split_hex_string(expected_cbor_hex, chunk_size=1024)
        if len(tx_body_chunks) == 1:
            header_lines.append(f'    .tx_body_cbor_hex = "{tx_body_chunks[0]}",')
        else:
            header_lines.append(f'    .tx_body_cbor_hex = "{tx_body_chunks[0]}"')
            for chunk in tx_body_chunks[1:-1]:
                header_lines.append(f'                         "{chunk}"')
            header_lines.append(f'                         "{tx_body_chunks[-1]}",')
        header_lines.append(f'    .expected_hash_hex = "{expected_hash_hex}",')
        header_lines.append(f"    .signing_mode = {int(test_case.signingMode)},")
        header_lines.append(f"    .network_id = {network_id_value},")
        header_lines.append(f"    .protocol_magic = {protocol_magic_value},")
        header_lines.append(f"    .num_inputs = {len(tx.inputs)},")
        header_lines.append(f"    .num_outputs = {len(tx.outputs)},")
        witness_paths = gather_witness_paths(
            tx,
            test_case.signingMode,
            getattr(test_case, "additionalWitnessPaths", []),
        )
        header_lines.append(f"    .num_witnesses = {len(witness_paths)},")
        header_lines.append(f"    .num_certificates = {len(tx.certificates) if tx.certificates else 0},")
        header_lines.append(f"    .num_withdrawals = {len(tx.withdrawals) if tx.withdrawals else 0},")
        header_lines.append(f"    .num_mint_asset_groups = {len(tx.mint) if tx.mint else 0},")
        header_lines.append(f"    .include_ttl = {_bool_to_c(tx.ttl is not None)},")
        header_lines.append(
            f"    .include_validity_interval_start = "
            f"{_bool_to_c(tx.validityIntervalStart is not None)},"
        )
        header_lines.append(f"    .include_aux_data_hash = {_bool_to_c(include_aux_data_hash)},")
        header_lines.append(f"    .aux_data_type = {aux_data_type},")
        if include_aux_data_hash and aux_data_type == int(TxAuxiliaryDataType.CIP36_REGISTRATION):
            header_lines.append(f"    .aux_data_init_payload = {init_payload_name},")
            header_lines.append(f"    .aux_data_init_payload_len = sizeof({init_payload_name}),")
            if aux_data_delegation_payloads:
                header_lines.append(f"    .aux_data_delegations = {delegations_name},")
                header_lines.append(f"    .aux_data_delegation_count = {len(aux_data_delegation_payloads)},")
            else:
                header_lines.append("    .aux_data_delegations = NULL,")
                header_lines.append("    .aux_data_delegation_count = 0,")
        else:
            header_lines.append("    .aux_data_init_payload = NULL,")
            header_lines.append("    .aux_data_init_payload_len = 0,")
            header_lines.append("    .aux_data_delegations = NULL,")
            header_lines.append("    .aux_data_delegation_count = 0,")
        header_lines.append(
            f"    .include_script_data_hash = {_bool_to_c(include_script_data_hash)},"
        )
        header_lines.append(
            f"    .num_collateral_inputs = "
            f"{len(tx.collateralInputs) if hasattr(tx, 'collateralInputs') and tx.collateralInputs else 0},"
        )
        header_lines.append(
            f"    .num_required_signers = "
            f"{len(tx.requiredSigners) if hasattr(tx, 'requiredSigners') and tx.requiredSigners else 0},"
        )
        header_lines.append(
            f"    .include_network_id = "
            f"{_bool_to_c(getattr(tx, 'includeNetworkId', False) if hasattr(tx, 'includeNetworkId') else False)},"
        )
        header_lines.append(
            f"    .include_collateral_output = "
            f"{_bool_to_c(getattr(tx, 'collateralOutput', None) is not None)},"
        )
        header_lines.append(
            f"    .include_total_collateral = "
            f"{_bool_to_c(getattr(tx, 'totalCollateral', None) is not None)},"
        )
        header_lines.append(
            f"    .num_reference_inputs = "
            f"{len(tx.referenceInputs) if hasattr(tx, 'referenceInputs') and tx.referenceInputs else 0},"
        )
        header_lines.append(
            f"    .num_voters = "
            f"{len(tx.votingProcedures) if hasattr(tx, 'votingProcedures') and tx.votingProcedures else 0},"
        )
        treasury_value = getattr(tx, "treasury", None)
        donation_value = getattr(tx, "donation", None)
        header_lines.append(f"    .include_treasury = {_bool_to_c(treasury_value is not None)},")
        header_lines.append(f"    .treasury = {treasury_value if treasury_value is not None else 0},")
        header_lines.append(f"    .include_donation = {_bool_to_c(donation_value is not None)},")
        header_lines.append(f"    .donation = {donation_value if donation_value is not None else 0},")

        if include_aux_data_hash and aux_data_hash_hex is not None:
            header_lines.append(f'    .aux_data_hash_hex = "{aux_data_hash_hex}",')
        else:
            header_lines.append("    .aux_data_hash_hex = NULL,")
        header_lines.append(f"    .options = {options_value},")
        header_lines.append("};")
        header_lines.append("")

    header_lines.append("#if defined(__clang__)")
    header_lines.append("#pragma clang diagnostic pop")
    header_lines.append("#elif defined(__GNUC__)")
    header_lines.append("#pragma GCC diagnostic pop")
    header_lines.append("#endif")
    header_lines.append("")

    header_content = "\n".join(header_lines)
    output_file = UNIT_TESTS_DIR / f"test_sign_tx_fixtures_{era_key.lower()}.h"
    output_file.write_text(header_content)

    fixture_count = _count_fixture_structs(header_content)
    if fixture_count != len(tests):
        raise ValueError(
            f"Fixture count mismatch for {era_key}: expected {len(tests)}, got {fixture_count}"
        )

    print()
    print(f"Generated: {output_file}")
    print(f"Total fixtures: {len(tests)}")


def generate_all_fixtures() -> None:
    sign_tx_data = _load_sign_tx_tests()
    era_tests = sign_tx_data["era_tests"]
    aux_data_classes = {
        "TxAuxiliaryDataCIP36": sign_tx_data["TxAuxiliaryDataCIP36"],
        "TxAuxiliaryDataType": sign_tx_data["TxAuxiliaryDataType"],
        "TxAuxiliaryDataHash": sign_tx_data["TxAuxiliaryDataHash"],
    }

    for era_key in sorted(era_tests.keys()):
        tests = era_tests[era_key]
        _generate_fixtures_for_era(era_key, tests, aux_data_classes)


ERA_TEST_FILE_MAP: Dict[str, Tuple[str, str, str]] = {
    "byron": ("test_sign_tx_fixtures_byron.h", "test_sign_tx_byron.c", "BYRON"),
    "shelley": ("test_sign_tx_fixtures_shelley.h", "test_sign_tx_shelley.c", "SHELLEY"),
    "mary": ("test_sign_tx_fixtures_mary.h", "test_sign_tx_mary.c", "MARY"),
    "allegra": ("test_sign_tx_fixtures_allegra.h", "test_sign_tx_allegra.c", "ALLEGRA"),
    "alonzo": ("test_sign_tx_fixtures_alonzo.h", "test_sign_tx_alonzo.c", "ALONZO"),
    "babbage": ("test_sign_tx_fixtures_babbage.h", "test_sign_tx_babbage.c", "BABBAGE"),
    "alonzo_catalyst": ("test_sign_tx_fixtures_alonzo_catalyst.h", "test_sign_tx_alonzo_catalyst.c", "ALONZO_CATALYST"),
    "alonzo_cip36": ("test_sign_tx_fixtures_alonzo_cip36.h", "test_sign_tx_alonzo_cip36.c", "ALONZO_CIP36"),
    "conway": ("test_sign_tx_fixtures_conway.h", "test_sign_tx_conway.c", "CONWAY"),
    "conway_voting": ("test_sign_tx_fixtures_conway_voting.h", "test_sign_tx_conway_voting.c", "CONWAY_VOTING"),
    "conway_without_certificates": (
        "test_sign_tx_fixtures_conway_without_certificates.h",
        "test_sign_tx_conway_without_certificates.c",
        "CONWAY_WITHOUT_CERTIFICATES",
    ),
    "shelley_certificates": (
        "test_sign_tx_fixtures_shelley_certificates.h",
        "test_sign_tx_shelley_certificates.c",
        "SHELLEY_CERTIFICATES",
    ),
    "multisig": ("test_sign_tx_fixtures_multisig.h", "test_sign_tx_multisig.c", "MULTISIG"),
    "pool_registration": (
        "test_sign_tx_fixtures_pool_registration.h",
        "test_sign_tx_pool_registration.c",
        "POOL_REGISTRATION",
    ),
}

ERA_COMMENT_OVERRIDES = {
    "conway_without_certificates": "CONWAY_WITHOUT_CERTIFICATES Era Tests",
    "alonzo_catalyst": "ALONZO_CATALYST Era Tests",
    "alonzo_cip36": "ALONZO_CIP36 Era Tests",
}


def _sanitize_test_name(name: str) -> str:
    lower = name.lower()
    cleaned = re.sub(r"[^a-z0-9_]+", "_", lower)
    cleaned = re.sub(r"_+", "_", cleaned).strip("_")
    return cleaned


def _extract_fixtures_from_header(fixture_path: Path) -> List[Tuple[str, str]]:
    content = fixture_path.read_text()
    fixtures: List[Tuple[str, str]] = []
    pattern = re.compile(r"static const tx_fixture_t (FIXTURE_[A-Z0-9_]+)\s*=\s*\{(.*?)\};", re.S)
    for match in pattern.finditer(content):
        fixture_name = match.group(1)
        body = match.group(2)
        name_match = re.search(r'\.name\s*=\s*"([^"]+)"', body)
        if not name_match:
            continue
        display_name = name_match.group(1)
        fixtures.append((fixture_name, display_name))
    return fixtures


def _build_test_functions(fixtures: Sequence[Tuple[str, str]]) -> Tuple[List[str], List[str]]:
    functions: List[str] = []
    names: List[str] = []
    for fixture_name, display_name in fixtures:
        func_suffix = _sanitize_test_name(display_name)
        if not func_suffix:
            raise ValueError(f"Unable to sanitize fixture name {display_name}")
        test_name = f"test_{func_suffix}"
        for suffix, expert_flag in [("expert_off", "false"), ("expert_on", "true")]:
            function_name = f"{test_name}_{suffix}"
            functions.append(
                "static void {function_name}(void **state) {{\n"
                "    (void) state;\n"
                "    run_fixture_with_expert_mode(&{fixture_name}, {expert_flag});\n"
                "}}".format(
                    function_name=function_name,
                    fixture_name=fixture_name,
                    expert_flag=expert_flag,
                )
            )
            names.append(function_name)
    return functions, names


def _build_main_function(test_names: Sequence[str], test_c_file: str) -> str:
    registrations = ",\n        ".join(f"cmocka_unit_test({name})" for name in test_names)
    return (
        "// ======================================================================\n"
        "// Main\n"
        "// ======================================================================\n\n"
        "int main(void) {\n"
        "    const struct CMUnitTest tests[] = {\n"
        f"        {registrations},\n"
        "    };\n"
        f"    return _cmocka_run_group_tests(\"{Path(test_c_file).stem}\", "
        "tests, ARRAY_LEN(tests), NULL, NULL);\n"
        "}\n"
    )


def _generate_complete_test_file(era: str, fixture_file: str, test_c_file: str, era_upper: str) -> None:
    fixture_path = UNIT_TESTS_DIR / fixture_file
    test_path = UNIT_TESTS_DIR / test_c_file

    if not fixture_path.exists():
        raise FileNotFoundError(f"Missing fixture header: {fixture_path}")

    existing = test_path.read_text()

    era_block_match = re.search(r"^// =+\n// ([^\n]+ Era Tests)\n// =+\n", existing, re.MULTILINE)
    if era_block_match:
        boilerplate = existing[:era_block_match.start()]
        era_heading = era_block_match.group(1)
    else:
        placeholder_match = re.search(
            r"^// Placeholder test - actual tests are generated from fixtures",
            existing,
            re.MULTILINE,
        )
        if not placeholder_match:
            raise ValueError(f"Could not find test section marker in {test_c_file}")
        boilerplate = existing[:placeholder_match.start()]
        era_heading = ERA_COMMENT_OVERRIDES.get(era, f"{era_upper} Era Tests")

    fixtures = _extract_fixtures_from_header(fixture_path)
    if not fixtures:
        raise ValueError(f"No fixtures found in {fixture_file}")

    test_functions, test_names = _build_test_functions(fixtures)
    expected_test_count = len(fixtures) * 2
    if len(test_names) != expected_test_count:
        raise ValueError(
            f"Test count mismatch for {test_c_file}: expected {expected_test_count}, got {len(test_names)}"
        )

    tests_block = "\n\n".join(test_functions)
    main_block = _build_main_function(test_names, test_c_file)

    era_comment_block = (
        "// ======================================================================\n"
        f"// {era_heading}\n"
        "// ======================================================================\n\n"
    )

    complete_file = (
        boilerplate.rstrip()
        + "\n\n"
        + era_comment_block
        + tests_block
        + "\n\n"
        + main_block
    )

    test_path.write_text(complete_file)
    print(f"Generated {test_c_file}: {len(fixtures)} tests")

def generate_test_runners() -> None:

    for era, (fixture_file, test_c_file, era_upper) in ERA_TEST_FILE_MAP.items():
        _generate_complete_test_file(era, fixture_file, test_c_file, era_upper)

    print("\nAll test files generated successfully!")


NODE_SCRIPT = REPO_ROOT / "unit-tests" / "export_sign_tx_rejects.js"
MOCK_USB_LOADER = REPO_ROOT / "unit-tests" / "mock_usb_loader.js"
GENERATED_REJECT_HEADER = REPO_ROOT / "unit-tests" / "test_sign_tx_fixtures_rejects.h"


def _build_node_command() -> Tuple[str, ...]:
    return (
        "bash",
        "-lc",
        f"source ~/.nvm/nvm.sh && nvm use {NODE_VERSION} >/dev/null && "
        f"cd {REPO_ROOT / '..' / 'ledgerjs-cardano-shelley'} && "
        f"NODE_OPTIONS=--require={MOCK_USB_LOADER} NODE_PATH=./node_modules "
        f"node {NODE_SCRIPT}",
    )


def _run_node_export() -> Dict[str, Any]:
    try:
        proc = subprocess.run(_build_node_command(), capture_output=True, text=True, check=True)
        return json.loads(proc.stdout)
    except subprocess.CalledProcessError as exc:
        print("STDOUT:", exc.stdout)
        print("STDERR:", exc.stderr)
        raise


SET_ORDER = [
    "transactionInitRejectTestCases",
    "addressParamsRejectTestCases",
    "certificateRejectTestCases",
    "certificateStakingRejectTestCases",
    "certificateStakePoolRetirementRejectTestCases",
    "withdrawalRejectTestCases",
    "witnessRejectTestCases",
    "singleAccountRejectTestCases",
    "collateralOutputRejectTestCases",
    "testsInvalidTokenBundleOrdering",
    "poolRegistrationOwnerRejectTestCases",
    "stakePoolRegistrationPoolIdRejectTestCases",
    "stakePoolRegistrationOwnerRejectTestCases",
    "outputRejectTestCases",
    "testsCVoteRegistrationRejects",
    "invalidCertificates",
    "invalidPoolMetadataTestCases",
    "invalidRelayTestCases",
]

SET_PREFIX = {
    "transactionInitRejectTestCases": "REJECT_INIT",
    "addressParamsRejectTestCases": "REJECT_ADDRESS",
    "certificateRejectTestCases": "REJECT_CERT",
    "certificateStakingRejectTestCases": "REJECT_CERT_STAKING",
    "certificateStakePoolRetirementRejectTestCases": "REJECT_CERT_POOL_RETIRE",
    "withdrawalRejectTestCases": "REJECT_WITHDRAWAL",
    "witnessRejectTestCases": "REJECT_WITNESS",
    "singleAccountRejectTestCases": "REJECT_SINGLE_ACCOUNT",
    "collateralOutputRejectTestCases": "REJECT_COLLATERAL_OUTPUT",
    "testsInvalidTokenBundleOrdering": "REJECT_MULTIASSET",
    "poolRegistrationOwnerRejectTestCases": "REJECT_POOL_OWNER",
    "stakePoolRegistrationPoolIdRejectTestCases": "REJECT_POOL_ID",
    "stakePoolRegistrationOwnerRejectTestCases": "REJECT_POOL_OWNER",
    "outputRejectTestCases": "REJECT_OUTPUT",
    "testsCVoteRegistrationRejects": "REJECT_CVOTE",
    "invalidCertificates": "REJECT_CERT_INVALID",
    "invalidPoolMetadataTestCases": "REJECT_POOL_METADATA",
    "invalidRelayTestCases": "REJECT_RELAY",
}

REJECT_REASON_SW: Dict[str, str] = {
    "InvalidDataReason.NETWORK_INVALID_NETWORK_ID": "SWO_INVALID_NETWORK_ID",
    "InvalidDataReason.NETWORK_INVALID_PROTOCOL_MAGIC": "SWO_INVALID_PROTOCOL_MAGIC",
    "InvalidDataReason.MULTIASSET_INVALID_TOKEN_BUNDLE_ORDERING": "SWO_TX_PARSING_FAIL_CANONICAL_ORDER",
    "InvalidDataReason.MULTIASSET_INVALID_TOKEN_BUNDLE_NOT_UNIQUE": "SWO_TX_PARSING_FAIL_CANONICAL_ORDER",
    "InvalidDataReason.MULTIASSET_INVALID_ASSET_GROUP_ORDERING": "SWO_TX_PARSING_FAIL_CANONICAL_ORDER",
    "InvalidDataReason.MULTIASSET_INVALID_ASSET_GROUP_NOT_UNIQUE": "SWO_TX_PARSING_FAIL_CANONICAL_ORDER",
    "InvalidDataReason.SIGN_MODE_ORDINARY__POOL_REGISTRATION_NOT_ALLOWED": "SWO_SECURITY_CONDITION_NOT_SATISFIED",
    "InvalidDataReason.SIGN_MODE_MULTISIG__POOL_REGISTRATION_NOT_ALLOWED": "SWO_SECURITY_CONDITION_NOT_SATISFIED",
    "InvalidDataReason.SIGN_MODE_PLUTUS__POOL_REGISTRATION_NOT_ALLOWED": "SWO_SECURITY_CONDITION_NOT_SATISFIED",
    "InvalidDataReason.SIGN_MODE_POOL_OPERATOR__SINGLE_POOL_REG_CERTIFICATE_REQUIRED": "SWO_TX_PARSING_FAIL_CERTIFICATES",
    "InvalidDataReason.SIGN_MODE_POOL_OWNER__SINGLE_POOL_REG_CERTIFICATE_REQUIRED": "SWO_TX_PARSING_FAIL_CERTIFICATES",
    "InvalidDataReason.CERTIFICATE_INVALID_POOL_KEY_HASH": "SWO_TX_PARSING_FAIL_CERTIFICATES",
    "InvalidDataReason.WITHDRAWAL_INVALID_ORDERING": "SWO_TX_PARSING_FAIL_WITHDRAWALS",
}


def _parse_enum(enum_cls: Type[Any], value: Any) -> Any:
    if isinstance(value, enum_cls):
        return value
    if isinstance(value, str):
        normalized = value.replace("-", "_").replace(" ", "_").upper()
        members = enum_cls.__members__
        if normalized in members:
            return enum_cls[normalized]
        suffix = normalized.split("_")[-1]
        if suffix in members:
            return enum_cls[suffix]
        raise KeyError(f"{normalized} not found in {enum_cls.__name__}")
    return enum_cls(value)


def _parse_credential_params_type(
    value: Any,
    credential_params_type_enum: Type[Any],
    credential_params_type_map: Dict[int, Any],
) -> Any:
    if isinstance(value, credential_params_type_enum):
        return value
    if isinstance(value, str):
        return _parse_enum(credential_params_type_enum, value)
    if isinstance(value, int):
        mapped = credential_params_type_map.get(value)
        if mapped is not None:
            return mapped
    raise ValueError(f"Unknown CredentialParamsType value: {value}")


def _build_reject_fixtures() -> str:
    _add_tests_to_sys_path()
    from application_client.app_def import AddressType, NetworkDesc  # type: ignore
    from application_client.command_builder import CommandBuilder, P1Type, gather_witness_paths  # type: ignore
    from standalone.input_files.derive_address import DeriveAddressTestCase, pointer_to_str  # type: ignore
    from standalone.input_files.signTx import (  # type: ignore
        AnchorParams,
        AssetGroup,
        Certificate,
        CertificateType,
        CredentialParams,
        CredentialParamsType,
        Datum,
        DatumType,
        Margin,
        MultiHostRelayParams,
        PoolKey,
        PoolKeyType,
        PoolMetadataParams,
        PoolRegistrationParams,
        PoolRetirementParams,
        Relay,
        RelayType,
        SingleHostHostnameRelayParams,
        SingleHostIpAddrRelayParams,
        StakeDelegationParams,
        StakeRegistrationParams,
        Token,
        Transaction,
        TransactionSigningMode,
        TxAuxiliaryData,
        TxAuxiliaryDataHash,
        TxAuxiliaryDataType,
        TxInput,
        TxOutputAlonzo,
        TxOutputBabbage,
        TxOutputDestination,
        TxOutputDestinationType,
        TxOutputFormat,
        TxRequiredSignerType,
        RequiredSigner,
        Withdrawal,
        Voter,
        VoterType,
        Vote,
        VoteOption,
        VoterVotes,
        VotingProcedure,
        GovActionId,
        ThirdPartyAddressParams,
    )

    exported = _run_node_export()
    unexpected_sets = sorted(set(exported.keys()) - set(SET_PREFIX.keys()))
    if unexpected_sets:
        raise ValueError(f"Unknown reject fixture sets: {', '.join(unexpected_sets)}")

    credential_params_type_map: Dict[int, CredentialParamsType] = {
        0: CredentialParamsType.KEY_PATH,
        1: CredentialParamsType.KEY_HASH,
        2: CredentialParamsType.SCRIPT_HASH,
    }

    tsigning_mode_map = {
        "ordinary_transaction": TransactionSigningMode.ORDINARY_TRANSACTION,
        "pool_registration_as_owner": TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
        "pool_registration_as_operator": TransactionSigningMode.POOL_REGISTRATION_AS_OPERATOR,
        "multisig_transaction": TransactionSigningMode.MULTISIG_TRANSACTION,
        "plutus_transaction": TransactionSigningMode.PLUTUS_TRANSACTION,
    }

    def to_bip32_path(path: Sequence[int]) -> str:
        hardened = 0x80000000
        components: List[str] = []
        for segment in path:
            if segment >= hardened:
                components.append(f"{segment - hardened}'")
            else:
                components.append(str(segment))
        return "m/" + "/".join(components)

    def convert_network(network_json: Dict[str, Any]) -> NetworkDesc:
        return NetworkDesc(networkId=network_json["networkId"], protocol=network_json["protocolMagic"])

    def convert_input(input_json: Dict[str, Any]) -> TxInput:
        path = input_json.get("path")
        return TxInput(
            txHashHex=input_json["txHashHex"],
            path=to_bip32_path(path) if path else None,
            outputIndex=int(input_json.get("outputIndex", 0)),
        )

    def convert_credential(credential: Dict[str, Any]) -> CredentialParams:
        ctype = _parse_credential_params_type(
            credential["type"],
            CredentialParamsType,
            credential_params_type_map,
        )
        if ctype == CredentialParamsType.KEY_PATH:
            path_value = credential.get("keyPath") or credential.get("path")
            if path_value is None:
                raise ValueError("Missing keyPath for KEY_PATH credential")
            return CredentialParams(type=ctype, keyValue=to_bip32_path(path_value))
        if ctype == CredentialParamsType.KEY_HASH:
            hash_value = credential.get("keyHashHex") or credential.get("hashHex")
            if hash_value is None:
                raise ValueError("Missing keyHashHex for KEY_HASH credential")
            return CredentialParams(type=ctype, keyValue=hash_value.lower())
        if ctype == CredentialParamsType.SCRIPT_HASH:
            script_hash = credential.get("scriptHashHex")
            if script_hash is None:
                raise ValueError("Missing scriptHashHex for SCRIPT_HASH credential")
            return CredentialParams(type=ctype, keyValue=script_hash.lower())
        raise ValueError(f"Unsupported credential type: {credential}")

    def convert_asset_group(group: Dict[str, Any]) -> AssetGroup:
        tokens = [
            Token(assetNameHex=token["assetNameHex"].lower(), amount=int(token["amount"]))
            for token in group["tokens"]
        ]
        return AssetGroup(policyIdHex=group["policyIdHex"].lower(), tokens=tokens)

    def convert_datum(datum_json: Dict[str, Any]) -> Datum:
        datum_type = _parse_enum(DatumType, datum_json["type"])
        if datum_type == DatumType.HASH:
            return Datum(type=datum_type, datumHex=datum_json["datumHashHex"].lower())
        return Datum(type=datum_type, datumHex=datum_json["datumHex"].lower())

    def convert_pool_key(pool_key_json: Dict[str, Any]) -> PoolKey:
        key_type = _parse_enum(PoolKeyType, pool_key_json["type"])
        params = pool_key_json["params"]
        if key_type == PoolKeyType.DEVICE_OWNED:
            path_value = params.get("path") or params.get("spendingPath") or params.get("stakingPath")
            if path_value is None:
                raise ValueError(f"Missing path for device owned pool key: {pool_key_json}")
            return PoolKey(type=key_type, key=to_bip32_path(path_value))
        hash_value = (
            params.get("keyHashHex")
            or params.get("rewardAccountHex")
            or params.get("stakingKeyHashHex")
            or params.get("stakingScriptHashHex")
        )
        if not hash_value:
            raise ValueError(f"Missing hash for third-party pool key: {pool_key_json}")
        return PoolKey(type=key_type, key=hash_value.lower())

    def convert_relay(relay_json: Dict[str, Any]) -> Relay:
        relay_type = _parse_enum(RelayType, relay_json["type"])
        params = relay_json["params"]
        if relay_type == RelayType.SINGLE_HOST_IP_ADDR:
            return Relay(
                type=relay_type,
                params=SingleHostIpAddrRelayParams(
                    portNumber=params.get("portNumber"),
                    ipv4=params.get("ipv4"),
                    ipv6=params.get("ipv6"),
                ),
            )
        if relay_type == RelayType.SINGLE_HOST_HOSTNAME:
            return Relay(
                type=relay_type,
                params=SingleHostHostnameRelayParams(portNumber=params["portNumber"], dnsName=params["dnsName"]),
            )
        return Relay(type=relay_type, params=MultiHostRelayParams(dnsName=params["dnsName"]))

    def convert_pool_registration_params(params_json: Dict[str, Any]) -> PoolRegistrationParams:
        margin_json = params_json["margin"]
        metadata_json = params_json.get("metadata")
        return PoolRegistrationParams(
            poolKey=convert_pool_key(params_json["poolKey"]),
            vrfKeyHashHex=params_json["vrfKeyHashHex"].lower(),
            pledge=int(params_json["pledge"]),
            cost=int(params_json["cost"]),
            margin=Margin(numerator=int(margin_json["numerator"]), denominator=int(margin_json["denominator"])),
            rewardAccount=convert_pool_key(params_json["rewardAccount"]),
            poolOwners=[convert_pool_key(owner) for owner in params_json["poolOwners"]],
            relays=[convert_relay(relay) for relay in params_json["relays"]],
            metadata=PoolMetadataParams(
                metadata_json["metadataUrl"], metadata_json["metadataHashHex"].lower()
            )
            if metadata_json
            else None,
        )

    def convert_certificate(cert_json: Dict[str, Any]) -> Certificate:
        cert_type = _parse_enum(CertificateType, cert_json["type"])
        params = cert_json["params"]
        if cert_type == CertificateType.STAKE_REGISTRATION:
            return Certificate(type=cert_type, params=StakeRegistrationParams(stakeCredential=convert_credential(params["stakeCredential"])))
        if cert_type == CertificateType.STAKE_DEREGISTRATION:
            return Certificate(type=cert_type, params=StakeRegistrationParams(stakeCredential=convert_credential(params["stakeCredential"])))
        if cert_type == CertificateType.STAKE_DELEGATION:
            return Certificate(
                type=cert_type,
                params=StakeDelegationParams(
                    stakeCredential=convert_credential(params["stakeCredential"]),
                    poolKeyHash=params["poolKeyHashHex"].lower(),
                ),
            )
        if cert_type == CertificateType.STAKE_POOL_REGISTRATION:
            return Certificate(type=cert_type, params=convert_pool_registration_params(params))
        if cert_type == CertificateType.STAKE_POOL_RETIREMENT:
            pool_key_path = params.get("poolKeyPath")
            if pool_key_path is None:
                pool_key_path = params.get("poolCredentialPath")
            if pool_key_path is None:
                raise ValueError("Missing poolKeyPath for pool retirement certificate")
            pool_credential = CredentialParams(
                type=CredentialParamsType.KEY_PATH,
                keyValue=to_bip32_path(pool_key_path),
            )
            return Certificate(
                type=cert_type,
                params=PoolRetirementParams(
                    poolCredential=pool_credential,
                    retirementEpoch=int(params["retirementEpoch"]),
                ),
            )
        raise ValueError(f"Unsupported certificate type: {cert_type}")

    def convert_withdrawal(withdraw_json: Dict[str, Any]) -> Withdrawal:
        return Withdrawal(
            stakeCredential=convert_credential(withdraw_json["stakeCredential"]),
            amount=int(withdraw_json["amount"]),
        )

    def convert_required_signers(signers_json: Sequence[Dict[str, Any]]) -> List[RequiredSigner]:
        result: List[RequiredSigner] = []
        for signer in signers_json:
            signer_type = _parse_enum(TxRequiredSignerType, signer["type"])
            if signer_type == TxRequiredSignerType.PATH:
                result.append(RequiredSigner(type=signer_type, pathOrHashHex=to_bip32_path(signer["path"])))
            else:
                result.append(RequiredSigner(type=signer_type, pathOrHashHex=signer["hashHex"].lower()))
        return result

    def convert_vote(vote_json: Dict[str, Any]) -> Vote:
        gov_action = vote_json["govActionId"]
        return Vote(
            govActionId=GovActionId(txHashHex=gov_action["txHashHex"], govActionIndex=int(gov_action["govActionIndex"])),
            votingProcedure=VotingProcedure(
                vote=_parse_enum(VoteOption, vote_json["votingProcedure"]["vote"]),
                anchor=AnchorParams(
                    url=vote_json["votingProcedure"]["anchor"]["url"],
                    hashHex=vote_json["votingProcedure"]["anchor"]["hashHex"].lower(),
                )
                if vote_json["votingProcedure"].get("anchor")
                else None,
            ),
        )

    def convert_voter(voter_json: Dict[str, Any]) -> Voter:
        return Voter(type=_parse_enum(VoterType, voter_json["type"]), keyValue=voter_json["keyValue"])

    def convert_voting_procedures(procedures_json: Sequence[Dict[str, Any]]) -> List[VoterVotes]:
        result: List[VoterVotes] = []
        for entry in procedures_json:
            result.append(
                VoterVotes(
                    voter=convert_voter(entry["voter"]),
                    votes=[convert_vote(v) for v in entry.get("votes", [])],
                )
            )
        return result

    def convert_device_owned_destination(network: NetworkDesc, dest_json: Dict[str, Any]) -> TxOutputDestination:
        params = dest_json["params"]["params"]
        addr_type = _parse_enum(AddressType, dest_json["params"]["type"])
        spending_value = ""
        if "spendingPath" in params:
            spending_value = to_bip32_path(params["spendingPath"])
        elif "spendingScriptHashHex" in params:
            spending_value = params["spendingScriptHashHex"].lower()

        staking_value = ""
        if "stakingPath" in params:
            staking_value = to_bip32_path(params["stakingPath"])
        elif "stakingKeyHashHex" in params:
            staking_value = params["stakingKeyHashHex"].lower()
        elif "stakingScriptHashHex" in params:
            staking_value = params["stakingScriptHashHex"].lower()
        elif "stakingBlockchainPointer" in params:
            pointer = params["stakingBlockchainPointer"]
            staking_value = pointer_to_str(pointer["blockIndex"], pointer["txIndex"], pointer["certificateIndex"])

        return TxOutputDestination(
            TxOutputDestinationType.DEVICE_OWNED,
            DeriveAddressTestCase("", network, addr_type, spending_value, staking_value),
        )

    def convert_destination(network: NetworkDesc, dest_json: Dict[str, Any]) -> TxOutputDestination:
        if dest_json["type"] == "third_party":
            return TxOutputDestination(
                TxOutputDestinationType.THIRD_PARTY,
                ThirdPartyAddressParams(dest_json["params"]["addressHex"].lower()),
            )
        return convert_device_owned_destination(network, dest_json)

    def convert_output(network: NetworkDesc, output_json: Dict[str, Any]) -> Any:
        destination = convert_destination(network, output_json["destination"])
        amount = int(output_json["amount"])
        token_bundle = [convert_asset_group(group) for group in output_json.get("tokenBundle", [])]
        datum = None
        if output_json.get("datum"):
            datum = convert_datum(output_json["datum"])
        elif output_json.get("datumHashHex"):
            datum = Datum(type=DatumType.HASH, datumHex=output_json["datumHashHex"].lower())

        reference_script = output_json.get("referenceScriptHex")
        if reference_script:
            reference_script = reference_script.lower()

        format_raw = output_json.get("format")
        format_value = _parse_enum(TxOutputFormat, format_raw) if format_raw is not None else TxOutputFormat.ARRAY_LEGACY

        if format_value == TxOutputFormat.MAP_BABBAGE:
            return TxOutputBabbage(
                destination,
                amount,
                format=format_value,
                tokenBundle=token_bundle,
                datum=datum,
                referenceScriptHex=reference_script,
            )
        return TxOutputAlonzo(
            destination,
            amount,
            format=format_value,
            tokenBundle=token_bundle,
            datum=datum,
        )

    def convert_auxiliary_data(data: Optional[Dict[str, Any]]) -> Optional[TxAuxiliaryData]:
        if data is None:
            return None
        aux_type = _parse_enum(TxAuxiliaryDataType, data["type"])
        if aux_type == TxAuxiliaryDataType.ARBITRARY_HASH:
            return TxAuxiliaryData(
                type=TxAuxiliaryDataType.ARBITRARY_HASH,
                params=TxAuxiliaryDataHash(data["params"]["hashHex"].lower()),
            )
        raise ValueError("Unsupported auxiliary data type")

    def convert_transaction(tx_json: Dict[str, Any]) -> Transaction:
        network = convert_network(tx_json["network"])
        return Transaction(
            network=network,
            inputs=[convert_input(inp) for inp in tx_json.get("inputs", [])],
            outputs=[convert_output(network, out) for out in tx_json.get("outputs", [])],
            fee=int(tx_json.get("fee", 0)),
            ttl=tx_json.get("ttl"),
            certificates=[convert_certificate(cert) for cert in tx_json.get("certificates", [])],
            withdrawals=[convert_withdrawal(w) for w in tx_json.get("withdrawals", [])],
            mint=[convert_asset_group(group) for group in tx_json.get("mint", [])],
            collateralInputs=[convert_input(inp) for inp in tx_json.get("collateralInputs", [])],
            requiredSigners=convert_required_signers(tx_json.get("requiredSigners", [])),
            referenceInputs=[convert_input(inp) for inp in tx_json.get("referenceInputs", [])],
            votingProcedures=convert_voting_procedures(tx_json.get("votingProcedures", [])),
            auxiliaryData=convert_auxiliary_data(tx_json.get("auxiliaryData")),
            validityIntervalStart=tx_json.get("validityIntervalStart"),
            scriptDataHash=tx_json.get("scriptDataHashHex"),
            includeNetworkId=tx_json.get("includeNetworkId"),
            collateralOutput=convert_output(network, tx_json["collateralOutput"])
            if tx_json.get("collateralOutput")
            else None,
            totalCollateral=tx_json.get("totalCollateral"),
            treasury=tx_json.get("treasury"),
            donation=tx_json.get("donation"),
        )

    def sanitize_name(name: str) -> str:
        result = []
        for char in name.replace("-", "_"):
            if char.isalnum():
                result.append(char.upper())
            else:
                result.append("_")
        cleaned = "_".join(part for part in "".join(result).split("_") if part)
        return cleaned

    def format_display_name(prefix: str, test_name: str, reason: Optional[str] = None) -> str:
        cleaned = test_name.replace("-", "").replace(" ", "_")
        cleaned = "_".join(part for part in cleaned.split("_") if part)
        if reason:
            reason_label = reason.split(".")[-1]
            reason_label = sanitize_name(reason_label)
            if reason_label:
                cleaned = f"{cleaned}_{reason_label}"
        return f"[{prefix}] {cleaned}"

    def to_hex_lines(hex_str: str, indent: int = 4, append_comma: bool = False) -> List[str]:
        chunk_size = 64
        lines = []
        for i in range(0, len(hex_str), chunk_size):
            segment = hex_str[i:i + chunk_size]
            lines.append(" " * indent + f"\"{segment}\"")
        if append_comma and lines:
            lines[-1] = lines[-1] + ","
        return lines

    def reject_reason_to_status_word(prefix: str,
                                     reason: Optional[str],
                                     fixture_name: str,
                                     tx: Transaction) -> str:
        if fixture_name == "Non-mainnet protocol magic":
            return "SWO_INVALID_PROTOCOL_MAGIC"
        if fixture_name == "Invalid network id":
            return "SWO_INVALID_NETWORK_ID"

        if prefix == "REJECT_ADDRESS":
            if "Pool operator - spending choice not path" in fixture_name or "Pool owner - unconditionally" in fixture_name:
                return "SWO_SECURITY_CONDITION_NOT_SATISFIED"

        if prefix == "REJECT_INIT":
            return "SWO_SECURITY_CONDITION_NOT_SATISFIED"

        if prefix == "REJECT_SINGLE_ACCOUNT":
            return "SWO_SECURITY_CONDITION_NOT_SATISFIED"

        if prefix == "REJECT_COLLATERAL_OUTPUT" and "inline datum" in fixture_name.lower():
            return "SWO_SECURITY_CONDITION_NOT_SATISFIED"

        if fixture_name == "Reject tx with invalid canonical ordering of withdrawals":
            return "SWO_TX_PARSING_FAIL_WITHDRAWALS"

        if reason and reason in REJECT_REASON_SW:
            sw = REJECT_REASON_SW[reason]
            if prefix == "REJECT_CERT" and sw == "SWO_TX_PARSING_FAIL_CERTIFICATES":
                if "Pool registration" in fixture_name or "Stake delegation" in fixture_name:
                    return "SWO_TX_PARSING_FAIL_CERTIFICATES"
                return "SWO_SECURITY_CONDITION_NOT_SATISFIED"
            return sw
        return "SWO_SECURITY_CONDITION_NOT_SATISFIED"

    @dataclass
    class ChunkInfo:
        p1: int
        more: bool
        hex_payload: str

    @dataclass
    class FixtureInfo:
        name: str
        display_name: str
        prefix: str
        sanitized_name: str
        init_hex: str
        chunks: List[ChunkInfo]
        expected_sw: str
        expect_init_failure: bool
        reject_reason: Optional[str]

    def build_fixture(fixture_json: Dict[str, Any], prefix: str) -> FixtureInfo:
        tx = convert_transaction(fixture_json["tx"])
        builder = CommandBuilder()
        signing_mode = tsigning_mode_map[fixture_json["signingMode"]]
        additional_paths = [
            to_bip32_path(path) if isinstance(path, list) else path
            for path in fixture_json.get("additionalWitnessPaths", [])
        ]
        if prefix == "REJECT_WITNESS":
            witness_paths = list(additional_paths)
        else:
            witness_paths = gather_witness_paths(tx, signing_mode, additional_paths)
        init_params = builder.build_tx_init_params(
            tx=tx,
            signing_mode=signing_mode,
            witness_paths=witness_paths,
        )
        init_payload = builder.sign_tx_init(init_params)[5:]
        chunks = [
            ChunkInfo(
                p1=chunk[2],
                
                more=chunk[2] != P1Type.P1_TX_CHUNK_LAST,

                hex_payload=chunk[5:].hex().upper(),
            )
            for chunk in builder.serialize_transaction_chunks(tx)
        ]
        if prefix in ("REJECT_WITNESS", "REJECT_SINGLE_ACCOUNT"):
            for path in witness_paths:
                witness_apdu = builder.sign_tx_witness(path)
                chunks.append(
                    ChunkInfo(
                        p1=P1Type.P1_TX_WITNESSES,
                        more=False,
                        hex_payload=witness_apdu[5:].hex().upper(),
                    )
                )
        reason = fixture_json.get("rejectReason")
        expect_init_failure = prefix == "REJECT_INIT"
        if prefix == "REJECT_ADDRESS":
            if "Pool operator - spending choice not path" in fixture_json["testName"] or "Pool owner - unconditionally" in fixture_json["testName"]:
                expect_init_failure = True

        display_name = format_display_name(prefix, fixture_json["testName"], reason)
        return FixtureInfo(
            name=fixture_json["testName"],
            display_name=display_name,
            prefix=prefix,
            sanitized_name=sanitize_name(fixture_json["testName"]),
            init_hex=init_payload.hex().upper(),
            chunks=chunks,
            expected_sw=reject_reason_to_status_word(prefix, reason, fixture_json["testName"], tx),
            expect_init_failure=expect_init_failure,
            reject_reason=reason,
        )

    def generate_header(fixtures: Dict[str, List[FixtureInfo]]) -> str:
        lines = [
            "// Auto-generated file. Do not edit directly.",
            "#pragma once",
            "",
            "#include <stdint.h>",
            "#include <stdbool.h>",
            "",
        ]
        for set_name in SET_ORDER:
            prefix = SET_PREFIX.get(set_name)
            if not prefix or set_name not in fixtures:
                continue
            for fixture in fixtures[set_name]:
                if not fixture.chunks:
                    continue
                if fixture.reject_reason:
                    lines.append(f"// {fixture.reject_reason}")
                lines.append(f"static const apdu_segment_t SIGN_TX_SEGMENTS_{prefix}_{fixture.sanitized_name}[] = {{")
                for chunk in fixture.chunks:
                    lines.append("    {")
                    lines.append("        .hex_payload =")
                    lines.extend(to_hex_lines(chunk.hex_payload, append_comma=True))
                    lines.append(f"        .p1 = 0x{chunk.p1:02X},")
                    lines.append(f"        .more = {'true' if chunk.more else 'false'},")
                    lines.append("    },")
                lines.append("};")
                lines.append("")
            lines.append("")
        lines.append("static const sign_tx_reject_fixture_t SIGN_TX_REJECT_FIXTURES[] = {")
        for set_name in SET_ORDER:
            prefix = SET_PREFIX.get(set_name)
            if not prefix or set_name not in fixtures:
                continue
            for fixture in fixtures[set_name]:
                if fixture.reject_reason:
                    lines.append(f"    // {fixture.reject_reason}")
                lines.append("    {")
                lines.append(f'        .name = "{fixture.display_name}",')
                lines.append("        .init_hex =")
                lines.extend(to_hex_lines(fixture.init_hex, indent=8, append_comma=True))
                if fixture.chunks:
                    array_name = f"SIGN_TX_SEGMENTS_{prefix}_{fixture.sanitized_name}"
                    lines.append(f"        .chunks = {array_name},")
                    lines.append(f"        .chunk_count = ARRAY_LEN({array_name}),")
                else:
                    lines.append("        .chunks = NULL,")
                    lines.append("        .chunk_count = 0,")
                lines.append(f"        .expected_sw = {fixture.expected_sw},")
                lines.append(f"        .expect_init_failure = {'true' if fixture.expect_init_failure else 'false'},")
                lines.append("        .skip_reason = NULL,")
                lines.append("    },")
        lines.append("};")
        lines.append("")
        return "\n".join(lines)

    fixtures: Dict[str, List[FixtureInfo]] = {}
    for set_name, entries in exported.items():
        prefix = SET_PREFIX.get(set_name)
        if not prefix:
            continue
        fixtures.setdefault(set_name, [])
        for entry in entries:
            fixtures[set_name].append(build_fixture(entry, prefix))

    return generate_header(fixtures)


def generate_reject_fixtures() -> None:
    header = _build_reject_fixtures()
    GENERATED_REJECT_HEADER.write_text(header)
    print(f"Generated {GENERATED_REJECT_HEADER}")


def regenerate_mock_data() -> None:
    try:
        from ragger.bip import calculate_public_key_and_chaincode, CurveChoice  # type: ignore
        from ragger.conftest import configuration as ragger_configuration  # type: ignore
        from bip_utils import Bip39SeedGenerator, Bip32Ed25519Kholaw  # type: ignore
        from nacl import bindings  # type: ignore
    except ImportError as exc:
        print(f"ERROR: missing dependency: {exc}")
        print("Please activate the venv: source ../tests/standalone/venv/bin/activate")
        sys.exit(1)

    default_mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"

    def resolve_mnemonic() -> str:
        if ragger_configuration is not None:
            optional_seed = getattr(ragger_configuration.OPTIONAL, "CUSTOM_SEED", "")
            if optional_seed:
                return optional_seed
        return default_mnemonic

    mnemonic = resolve_mnemonic()

    def parse_bip32_path_from_c_array(path_array_str: str) -> str:
        hex_values = re.findall(r"0x[0-9a-fA-F]+", path_array_str)
        path_parts = ["m"]
        for hex_val in hex_values:
            val = int(hex_val, 16)
            if val & 0x80000000:
                path_parts.append(f"{val & 0x7FFFFFFF}'")
            else:
                path_parts.append(str(val))
        return "/".join(path_parts)

    def format_c_array(data: bytes, indent: str = "      ") -> str:
        chunks = [data[i:i + 8] for i in range(0, len(data), 8)]
        lines = []
        for i, chunk in enumerate(chunks):
            prefix = "" if i == 0 else indent + " "
            lines.append(prefix + "0x" + ", 0x".join(f"{b:02x}" for b in chunk))
        return (",\n" + indent).join(lines)

    input_file = UNIT_TESTS_DIR / "mocks" / "crypto_mock_data.h"
    temp_output_file = UNIT_TESTS_DIR / "mocks" / "crypto_mock_data_regenerated.h"

    content = input_file.read_text()

    print("Regenerating mock data from standard test mnemonic...")
    print(f"Input:  {input_file}")
    print(f"Output: {temp_output_file}\n")

    entry_match_count = 0

    def regenerate_entry(match: re.Match[str]) -> str:
        nonlocal entry_match_count
        entry_match_count += 1
        path_desc = match.group(2)
        path_array = match.group(3)

        bip32_path = parse_bip32_path_from_c_array(path_array)

        try:
            derived_pk_hex, derived_cc_hex = calculate_public_key_and_chaincode(
                CurveChoice.Ed25519Kholaw,
                bip32_path,
                mnemonic=mnemonic
            )

            derived_pk = bytes.fromhex(derived_pk_hex[2:])
            derived_cc = bytes.fromhex(derived_cc_hex)
            derived_kh = hashlib.blake2b(derived_pk, digest_size=28).digest()

            print(f"✓ {path_desc} ({bip32_path})")

            entry_text = match.group(0)
            entry_text = re.sub(
                r'/\* Public key \(hex\): "[^"]*" \*/',
                f'/* Public key (hex): "{derived_pk.hex()}" */',
                entry_text
            )
            entry_text = re.sub(
                r"\.public_key = \{[^}]+\}",
                f".public_key = {{{format_c_array(derived_pk)}}}",
                entry_text
            )
            entry_text = re.sub(
                r'/\* Chain code \(hex\): "[^"]*" \*/',
                f'/* Chain code (hex): "{derived_cc.hex()}" */',
                entry_text
            )
            entry_text = re.sub(
                r"\.chain_code = \{[^}]+\}",
                f".chain_code = {{{format_c_array(derived_cc)}}}",
                entry_text
            )
            entry_text = re.sub(
                r"/\* Blake2b-224 key hash: [a-f0-9]+ \*/",
                f"/* Blake2b-224 key hash: {derived_kh.hex()} */",
                entry_text
            )
            entry_text = re.sub(
                r"\.key_hash = \{[^}]+\}",
                f".key_hash = {{{format_c_array(derived_kh)}}}",
                entry_text
            )
            return entry_text

        except Exception as exc:
            print(f"✗ {path_desc} ({bip32_path}): {exc}")
            return match.group(0)

    pattern = r'(/\* Path "([^"]+)"[^{]+\{ \.path = (\{[^}]+\}).*?\.key_hash = \{[^}]+\},\n    \},)'
    new_content = re.sub(pattern, regenerate_entry, content, flags=re.DOTALL)

    if entry_match_count == 0:
        raise ValueError("No mock path entries were regenerated")

    message_pattern = r"static const uint8_t (\w+)\[\] = \{([^}]+)\};"
    messages: Dict[str, bytes] = {}
    for match in re.finditer(message_pattern, new_content, flags=re.DOTALL):
        name = match.group(1)
        hex_values = re.findall(r"0x[0-9a-fA-F]{2}", match.group(2))
        if not hex_values:
            continue
        messages[name] = bytes(int(value, 16) for value in hex_values)

    seed = Bip39SeedGenerator(mnemonic).Generate()

    def sign_with_extended_key(extended_key: bytes, message: bytes) -> bytes:
        if len(extended_key) != 64:
            raise ValueError(f"Unexpected extended key length {len(extended_key)}")
        secret_scalar = extended_key[:32]
        prefix = extended_key[32:64]

        r_hash = hashlib.sha512(prefix + message).digest()
        r_scalar = bindings.crypto_core_ed25519_scalar_reduce(r_hash)
        r_point = bindings.crypto_scalarmult_ed25519_base_noclamp(r_scalar)

        public_key = bindings.crypto_scalarmult_ed25519_base_noclamp(secret_scalar)
        k_hash = hashlib.sha512(r_point + public_key + message).digest()
        k_scalar = bindings.crypto_core_ed25519_scalar_reduce(k_hash)

        k_times_a = bindings.crypto_core_ed25519_scalar_mul(k_scalar, secret_scalar)
        s_scalar = bindings.crypto_core_ed25519_scalar_add(r_scalar, k_times_a)

        return r_point + s_scalar

    def derive_signature(path_array: str, message_name: str) -> bytes:
        if message_name not in messages:
            raise ValueError(f"Missing message buffer {message_name}")
        bip32_path = parse_bip32_path_from_c_array(path_array)
        child = Bip32Ed25519Kholaw.FromSeed(seed).DerivePath(bip32_path)
        extended_key = child.PrivateKey().Raw().ToBytes()
        return sign_with_extended_key(extended_key, messages[message_name])

    signature_pattern = (
        r'(/\* Path "([^"]+)" message ([^*]+?) \*/\s*'
        r'\{ \.path = (\{[^}]+\}), \.path_len = [^,]+,\s*'
        r'\.message = ([^,]+),\s*\.message_len = [^,]+,\s*'
        r'/\* Signature \(hex\): "([^"]*)" \*/\s*'
        r'\.signature = \{([^}]+)\},\s*'
        r'\},)'
    )

    signature_match_count = 0

    def regenerate_signature_entry(match: re.Match[str]) -> str:
        nonlocal signature_match_count
        signature_match_count += 1
        message_desc = match.group(3)
        message_name = message_desc.split()[0]
        path_array = match.group(4)
        bip32_path = parse_bip32_path_from_c_array(path_array)

        message_bytes = messages.get(message_name, b"")
        message_hex = message_bytes.hex()
        print(f"✓ Signature {message_name} ({bip32_path})")
        print(f"  message={message_hex}")

        signature = derive_signature(path_array, message_name)
        signature_hex = signature.hex()

        entry_text = match.group(0)
        entry_text = re.sub(
            r'/\* Signature \(hex\): "[^"]*" \*/',
            f'/* Signature (hex): "{signature_hex}" */',
            entry_text
        )
        entry_text = re.sub(
            r"\.signature = \{[^}]+\}",
            f".signature = {{{format_c_array(signature)}}}",
            entry_text
        )
        return entry_text

    new_content = re.sub(signature_pattern, regenerate_signature_entry, new_content, flags=re.DOTALL)

    if signature_match_count == 0:
        raise ValueError("No signature entries were regenerated")

    temp_output_file.write_text(new_content)
    temp_output_file.replace(input_file)

    print(f"\n✓ Regenerated mock data written to: {input_file}")


def run_all() -> None:
    generate_all_fixtures()
    generate_test_runners()
    generate_reject_fixtures()
    regenerate_mock_data()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate unit-test fixtures from ragger/LedgerJS sources."
    )
    subparsers = parser.add_subparsers(dest="command")
    subparsers.add_parser("all", help="Run all generators (default).")
    subparsers.add_parser("fixtures", help="Generate sign-tx fixture headers.")
    subparsers.add_parser("generate-test-runners", help="Regenerate test_sign_tx_*.c files.")
    subparsers.add_parser("rejects", help="Generate reject fixture headers.")
    subparsers.add_parser("mock-data", help="Regenerate mocks/crypto_mock_data.h.")

    args = parser.parse_args()

    if args.command in (None, "all"):
        run_all()
    elif args.command == "fixtures":
        generate_all_fixtures()
    elif args.command == "generate-test-runners":
        generate_test_runners()
    elif args.command == "rejects":
        generate_reject_fixtures()
    elif args.command == "mock-data":
        regenerate_mock_data()
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()
