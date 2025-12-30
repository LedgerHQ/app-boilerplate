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
import types

ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
ALPHABET_INDEX = {char: index for index, char in enumerate(ALPHABET)}


def _ensure_base58_module():
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

    def b58decode(value):
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


_ensure_base58_module()

# Add test paths
sys.path.insert(0, str(Path(__file__).parent.parent / "tests"))
sys.path.insert(0, str(Path(__file__).parent.parent / "tests" / "application_client"))
sys.path.insert(0, str(Path(__file__).parent.parent / "tests" / "standalone"))

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
from application_client.command_builder import CommandBuilder, gather_witness_paths

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

def split_hex_string(hex_str: str, chunk_size: int = 1024) -> list[str]:
    """Split a hexadecimal string into smaller chunks."""
    return [hex_str[i:i + chunk_size] for i in range(0, len(hex_str), chunk_size)]

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
        "#if defined(__clang__)",
        "#pragma clang diagnostic push",
        "#pragma clang diagnostic ignored \"-Woverlength-strings\"",
        "#elif defined(__GNUC__)",
        "#pragma GCC diagnostic push",
        "#pragma GCC diagnostic ignored \"-Woverlength-strings\"",
        "#endif",
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

        include_script_data_hash = getattr(tx, "scriptDataHash", None) is not None

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
        tx_body_chunks = split_hex_string(expected_cbor_hex, chunk_size=1024)
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
        witness_paths = gather_witness_paths(tx, getattr(test_case, "additionalWitnessPaths", []))
        header_lines.append(f"    .num_witnesses = {len(witness_paths)},")
        header_lines.append(f"    .num_certificates = {len(tx.certificates) if tx.certificates else 0},")
        header_lines.append(f"    .num_withdrawals = {len(tx.withdrawals) if tx.withdrawals else 0},")
        header_lines.append(f"    .num_mint_asset_groups = {len(tx.mint) if tx.mint else 0},")
        header_lines.append(f"    .include_ttl = {bool_to_c(tx.ttl is not None)},")
        header_lines.append(f"    .include_validity_interval_start = {bool_to_c(tx.validityIntervalStart is not None)},")
        header_lines.append(f"    .include_aux_data_hash = {bool_to_c(include_aux_data_hash)},")
        header_lines.append(f"    .include_script_data_hash = {bool_to_c(include_script_data_hash)},")
        header_lines.append(f"    .num_collateral_inputs = {len(tx.collateralInputs) if hasattr(tx, 'collateralInputs') and tx.collateralInputs else 0},")
        header_lines.append(f"    .num_required_signers = {len(tx.requiredSigners) if hasattr(tx, 'requiredSigners') and tx.requiredSigners else 0},")
        header_lines.append(f"    .include_network_id = {bool_to_c(getattr(tx, 'includeNetworkId', False) if hasattr(tx, 'includeNetworkId') else False)},")
        header_lines.append(f"    .include_collateral_output = {bool_to_c(getattr(tx, 'collateralOutput', None) is not None)},")
        header_lines.append(f"    .include_total_collateral = {bool_to_c(getattr(tx, 'totalCollateral', None) is not None)},")
        header_lines.append(f"    .num_reference_inputs = {len(tx.referenceInputs) if hasattr(tx, 'referenceInputs') and tx.referenceInputs else 0},")
        header_lines.append(f"    .num_voters = {len(tx.votingProcedures) if hasattr(tx, 'votingProcedures') and tx.votingProcedures else 0},")
        treasury_value = getattr(tx, 'treasury', None)
        donation_value = getattr(tx, 'donation', None)
        header_lines.append(f"    .include_treasury = {bool_to_c(treasury_value is not None)},")
        header_lines.append(f"    .treasury = {treasury_value if treasury_value is not None else 0},")
        header_lines.append(f"    .include_donation = {bool_to_c(donation_value is not None)},")
        header_lines.append(f"    .donation = {donation_value if donation_value is not None else 0},")

        if include_aux_data_hash and aux_data_hash_hex is not None:
            header_lines.append(f'    .aux_data_hash_hex = "{aux_data_hash_hex}",')
        else:
            header_lines.append(f"    .aux_data_hash_hex = NULL,")
        header_lines.append(f"    .options = {options_value},")
        header_lines.append("};")
        header_lines.append("")

    header_lines.append("#if defined(__clang__)")
    header_lines.append("#pragma clang diagnostic pop")
    header_lines.append("#elif defined(__GNUC__)")
    header_lines.append("#pragma GCC diagnostic pop")
    header_lines.append("#endif")
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
