#!/usr/bin/env python3
"""
Generate C fixture for a given test case from LedgerJS test data.

Usage:
  python3 generate_fixture.py <era> <test_index>

Examples:
  python3 generate_fixture.py shelley 0    # Sign_tx_without_outputs
  python3 generate_fixture.py mary 0       # Sign_tx_with_a_multiasset_output
"""

import sys
from pathlib import Path
import hashlib

# Add test paths
sys.path.insert(0, str(Path(__file__).parent.parent / "tests"))
sys.path.insert(0, str(Path(__file__).parent.parent / "tests" / "application_client"))

from standalone.input_files.signTx import testsMary, testsShelleyNoCertificates, testsByron
from application_client.command_builder import CommandBuilder

# Map era names to test collections
ERA_TESTS = {
    'mary': testsMary,
    'shelley': testsShelleyNoCertificates,
    'byron': testsByron,
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

def cbor_hex_to_bytes(hex_str: str) -> bytes:
    """Convert hex string to bytes."""
    # Remove any whitespace
    hex_str = hex_str.replace(' ', '').replace('\n', '')
    return bytes.fromhex(hex_str)

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 generate_fixture.py <era> <test_index>")
        print()
        print("Available eras:", ", ".join(ERA_TESTS.keys()))
        print()
        print("Examples:")
        print("  python3 generate_fixture.py shelley 0")
        print("  python3 generate_fixture.py mary 0")
        sys.exit(1)

    era = sys.argv[1].lower()
    try:
        test_index = int(sys.argv[2])
    except ValueError:
        print(f"Error: test_index must be an integer, got '{sys.argv[2]}'")
        sys.exit(1)

    if era not in ERA_TESTS:
        print(f"Error: unknown era '{era}'")
        print(f"Available eras: {', '.join(ERA_TESTS.keys())}")
        sys.exit(1)

    tests = ERA_TESTS[era]
    if test_index >= len(tests):
        print(f"Error: test_index {test_index} out of range (0-{len(tests)-1})")
        sys.exit(1)

    test_case = tests[test_index]
    print(f"Generating C fixture for {era.upper()} test #{test_index}...")
    print(f"Test name: {test_case.name}")
    print()

    tx = test_case.tx
    signing_mode = test_case.signingMode

    # Serialize using Python client
    builder = CommandBuilder()
    raw_tx_bytes = builder._serialize_transaction_unpacked_raw(tx)

    print(f"Raw TX buffer size: {len(raw_tx_bytes)} bytes")
    print()

    # Expected CBOR and hash from test case
    expected_cbor_hex = test_case.txBody

    # Compute hash from CBOR
    cbor_bytes = cbor_hex_to_bytes(expected_cbor_hex)
    expected_hash_hex = compute_blake2b_256(cbor_bytes)

    print(f"Expected hash (computed): {expected_hash_hex}")
    print()

    # Generate C identifier names
    safe_name = sanitize_name_for_c(test_case.name)
    fixture_prefix = f"FIXTURE_{era.upper()}_{safe_name}"

    # Generate C header content
    header_content = f"""// Auto-generated fixture for "{test_case.name}" test
// Era: {era.upper()}
// Index: {test_index}
//
// Expected tx hash: {expected_hash_hex}
// Expected tx body CBOR: {expected_cbor_hex}

#pragma once

#include <stdint.h>
#include <stddef.h>

// Raw transaction buffer (unpacked binary format, NOT CBOR)
// This is the format that tx_parse.c expects
{format_bytes_as_c_array(raw_tx_bytes, f'{fixture_prefix}_RAW_TX')}

#define {fixture_prefix}_RAW_TX_LEN {len(raw_tx_bytes)}

// Expected transaction body in CBOR format (for hash verification)
#define {fixture_prefix}_TX_BODY_CBOR_HEX \\
    "{expected_cbor_hex}"

// Expected transaction hash (blake2b-256 of CBOR)
#define {fixture_prefix}_EXPECTED_HASH_HEX \\
    "{expected_hash_hex}"

// Transaction metadata for INIT APDU
#define {fixture_prefix}_NUM_INPUTS {len(tx.inputs)}
#define {fixture_prefix}_NUM_OUTPUTS {len(tx.outputs)}
#define {fixture_prefix}_NUM_WITNESSES {test_case.signingMode}
#define {fixture_prefix}_INCLUDE_TTL {str(tx.ttl is not None).lower()}
#define {fixture_prefix}_INCLUDE_VALIDITY_INTERVAL_START {str(tx.validityIntervalStart is not None).lower()}
#define {fixture_prefix}_NUM_CERTIFICATES {len(tx.certificates) if tx.certificates else 0}
#define {fixture_prefix}_NUM_WITHDRAWALS {len(tx.withdrawals) if tx.withdrawals else 0}
#define {fixture_prefix}_NUM_MINT_ASSET_GROUPS {len(tx.mint) if tx.mint else 0}
"""

    # Write to file
    output_file = Path(__file__).parent / f"test_sign_tx_fixtures_{era.lower()}.h"
    output_file.write_text(header_content)

    print(f"Generated: {output_file}")
    print()
    print("Raw TX buffer (hex):")
    print(raw_tx_bytes.hex())
    print()
    print("Expected CBOR:")
    print(expected_cbor_hex)
    print()
    print("Expected hash:")
    print(expected_hash_hex)

if __name__ == "__main__":
    main()
