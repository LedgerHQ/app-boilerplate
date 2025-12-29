#!/usr/bin/env python3
"""
Generate all C fixtures for a given era into a single header file.

Usage:
  python3 generate_all_fixtures.py <era>

Examples:
  python3 generate_all_fixtures.py mary    # Generate all Mary tests
  python3 generate_all_fixtures.py shelley # Generate all Shelley tests
  python3 generate_all_fixtures.py byron   # Generate all Byron tests
"""

import sys
from pathlib import Path
import hashlib

# Add test paths
sys.path.insert(0, str(Path(__file__).parent.parent / "tests"))
sys.path.insert(0, str(Path(__file__).parent.parent / "tests" / "application_client"))

from standalone.input_files.signTx import (
    testsMary,
    testsShelleyNoCertificates,
    testsShelleyWithCertificates,
    testsAllegra,
    testsByron,
    testsAlonzo,
    testsBabbage,
    testsConwayWithCertificates,
    testsConwayWithoutCertificates,
    testsConwayVotingProcedures,
    testsMultisig,
    TxAuxiliaryDataType,
    TxAuxiliaryDataHash,
)
from application_client.command_builder import CommandBuilder

# Map era names to test collections
ERA_TESTS = {
    'byron': testsByron,
    'shelley': testsShelleyNoCertificates,
    'shelley_certificates': testsShelleyWithCertificates,
    'allegra': testsAllegra,
    'mary': testsMary,
    'alonzo': testsAlonzo,
    'babbage': testsBabbage,
    'conway': testsConwayWithCertificates,
    'conway_without_certificates': testsConwayWithoutCertificates,
    'conway_voting': testsConwayVotingProcedures,
    'multisig': testsMultisig,
}

def format_bytes_as_c_array(data: bytes, name: str, bytes_per_line: int = 16) -> str:
    """Format bytes as C array initializer."""
    lines = []
    for i in range(0, len(data), bytes_per_line):
        chunk = data[i:i+bytes_per_line]
        hex_bytes = ', '.join(f'0x{b:02X}' for b in chunk)
        lines.append(f'    {hex_bytes},')

    result = f'static const uint8_t {name}[] = {{\n'
    result += '\n'.join(lines)
    result += '\n};'
    return result

def compute_blake2b_256(data: bytes) -> str:
    """Compute blake2b-256 hash and return as hex string."""
    h = hashlib.blake2b(data, digest_size=32)
    return h.hexdigest()

def sanitize_name_for_c(name: str) -> str:
    """Convert test name to C-safe identifier."""
    # Replace special characters with underscores
    safe = ''.join(c if c.isalnum() else '_' for c in name)
    # Remove duplicate underscores
    while '__' in safe:
        safe = safe.replace('__', '_')
    return safe.upper()

def bool_to_c(value: bool) -> str:
    """Convert Python bool to lowercase C bool literal."""
    return "true" if value else "false"

def cbor_hex_to_bytes(hex_str: str) -> bytes:
    """Convert hex string to bytes."""
    # Remove any whitespace
    hex_str = hex_str.replace(' ', '').replace('\n', '')
    return bytes.fromhex(hex_str)

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 generate_all_fixtures.py <era>")
        print()
        print("Available eras:", ", ".join(ERA_TESTS.keys()))
        sys.exit(1)

    era = sys.argv[1].lower()

    if era not in ERA_TESTS:
        print(f"Error: unknown era '{era}'")
        print(f"Available eras: {', '.join(ERA_TESTS.keys())}")
        sys.exit(1)

    tests = ERA_TESTS[era]
    print(f"Generating C fixtures for {era.upper()} era ({len(tests)} tests)...")
    print()

    # Generate header content for all fixtures
    header_lines = [
        f"// Auto-generated fixtures for {era.upper()} era transaction tests",
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
    ]

    # Generate fixture for each test
    for test_index, test_case in enumerate(tests):
        print(f"  [{test_index + 1}/{len(tests)}] {test_case.name}...")

        tx = test_case.tx
        builder = CommandBuilder()
        raw_tx_bytes = builder._serialize_transaction_unpacked_raw(tx)

        # Expected CBOR and hash from test case
        expected_cbor_hex = test_case.txBody
        cbor_bytes = cbor_hex_to_bytes(expected_cbor_hex)
        expected_hash_hex = compute_blake2b_256(cbor_bytes)

        # Generate C identifier names
        safe_name = sanitize_name_for_c(test_case.name)
        fixture_prefix = f"FIXTURE_{era.upper()}_{safe_name}"

        include_aux_data_hash = (
            tx.auxiliaryData is not None and
            tx.auxiliaryData.type == TxAuxiliaryDataType.ARBITRARY_HASH
        )
        aux_data_hash_hex = None
        if include_aux_data_hash:
            aux_params = tx.auxiliaryData.params
            if isinstance(aux_params, TxAuxiliaryDataHash):
                aux_data_hash_hex = aux_params.hashHex
            else:
                include_aux_data_hash = False

        options_value = "TX_OPTIONS_TAG_CBOR_SETS" if "d90102" in expected_cbor_hex.lower() else "0"
        network_id_value = int(test_case.tx.network.networkId)
        protocol_magic_value = int(test_case.tx.network.protocol)

        # Add fixture comment
        header_lines.append(f"// Test {test_index}: {test_case.name}")
        header_lines.append("//")

        # Add raw TX array
        array_lines = format_bytes_as_c_array(raw_tx_bytes, f'{fixture_prefix}_RAW_TX').split('\n')
        header_lines.extend(array_lines)
        header_lines.append("")
        # Add fixture struct
        header_lines.append(f"static const tx_fixture_t {fixture_prefix} = {{")
        header_lines.append(f'    .name = "{test_case.name}",')
        header_lines.append(f"    .raw_tx = {fixture_prefix}_RAW_TX,")
        header_lines.append(f"    .raw_tx_len = sizeof({fixture_prefix}_RAW_TX),")
        header_lines.append(f'    .tx_body_cbor_hex = "{expected_cbor_hex}",')
        header_lines.append(f'    .expected_hash_hex = "{expected_hash_hex}",')
        header_lines.append(f"    .network_id = {network_id_value},")
        header_lines.append(f"    .protocol_magic = {protocol_magic_value},")
        header_lines.append(f"    .num_inputs = {len(tx.inputs)},")
        header_lines.append(f"    .num_outputs = {len(tx.outputs)},")
        header_lines.append(f"    .num_witnesses = {test_case.signingMode},")
        header_lines.append(f"    .num_certificates = {len(tx.certificates) if tx.certificates else 0},")
        header_lines.append(f"    .num_withdrawals = {len(tx.withdrawals) if tx.withdrawals else 0},")
        header_lines.append(f"    .num_mint_asset_groups = {len(tx.mint) if tx.mint else 0},")
        header_lines.append(f"    .include_ttl = {bool_to_c(tx.ttl is not None)},")
        header_lines.append(f"    .include_validity_interval_start = {bool_to_c(tx.validityIntervalStart is not None)},")
        header_lines.append(f"    .include_aux_data_hash = {bool_to_c(include_aux_data_hash)},")
        if include_aux_data_hash and aux_data_hash_hex is not None:
            header_lines.append(f'    .aux_data_hash_hex = "{aux_data_hash_hex}",')
        else:
            header_lines.append(f"    .aux_data_hash_hex = NULL,")
        header_lines.append(f"    .options = {options_value},")
        header_lines.append("};")
        header_lines.append("")

    # Write to file
    header_content = '\n'.join(header_lines)
    output_file = Path(__file__).parent / f"test_sign_tx_fixtures_{era.lower()}.h"
    output_file.write_text(header_content)

    print()
    print(f"Generated: {output_file}")
    print(f"Total fixtures: {len(tests)}")

if __name__ == "__main__":
    main()
