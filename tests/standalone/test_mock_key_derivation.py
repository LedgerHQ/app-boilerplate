"""
Test that verifies unit test mock key derivation data matches the standard test mnemonic.

This ensures that hardcoded mock data in unit-tests/mocks/crypto_mock_data.h
stays synchronized with actual key derivation from the standard test mnemonic:
"abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"

The test dynamically parses the C header file and verifies all mock entries.
"""

import pytest
import hashlib
import re
from pathlib import Path

from ragger.bip import calculate_public_key_and_chaincode, CurveChoice
from ragger.backend import BackendInterface

# Standard test mnemonic used in all unit tests
MNEMONIC = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"


def parse_bip32_path_from_c_array(path_array_str: str) -> str:
    """
    Convert C array representation to BIP32 path string.
    E.g., "{0x8000073c, 0x80000717, 0x80000000, 0x00000003, 0x00000000}"
    -> "m/1852'/1815'/0'/3/0"
    """
    # Extract hex values
    hex_values = re.findall(r'0x[0-9a-fA-F]+', path_array_str)

    path_parts = ["m"]
    for hex_val in hex_values:
        val = int(hex_val, 16)
        # Check if hardened (bit 31 set)
        if val & 0x80000000:
            path_parts.append(f"{val & 0x7FFFFFFF}'")
        else:
            path_parts.append(str(val))

    return "/".join(path_parts)


def parse_hex_array_from_c(c_array_str: str) -> bytes:
    """
    Parse C byte array into Python bytes.
    E.g., "{0xba, 0x41, 0xc5, ...}" -> bytes
    """
    hex_values = re.findall(r'0x[0-9a-fA-F]{2}', c_array_str)
    return bytes(int(h, 16) for h in hex_values)


def parse_mock_paths_from_header():
    """
    Parse all mock path entries from crypto_mock_data.h.
    Returns list of dicts with path, public_key, chain_code, key_hash, and description.
    """
    # Find the header file
    test_dir = Path(__file__).parent
    header_file = test_dir.parent.parent / "unit-tests" / "mocks" / "crypto_mock_data.h"

    if not header_file.exists():
        pytest.skip(f"Mock data header not found: {header_file}")

    with open(header_file, 'r') as f:
        content = f.read()

    # Find all mock path entries
    # Pattern matches from "/* Path " to the closing "},"
    entry_pattern = r'/\* Path "([^"]+)"[^{]+\{([^}]+\}[^}]+\})'

    entries = []

    # Split content by entries - find each mock path block
    blocks = re.findall(
        r'/\* Path "([^"]+)".*?\.path = (\{[^}]+\}).*?\.public_key = (\{[^}]+\}).*?\.chain_code = (\{[^}]+\}).*?\.key_hash = (\{[^}]+\})',
        content,
        re.DOTALL
    )

    for path_desc, path_array, pubkey_array, chaincode_array, keyhash_array in blocks:
        # Parse the BIP32 path
        bip32_path = parse_bip32_path_from_c_array(path_array)

        # Parse the binary data
        public_key = parse_hex_array_from_c(pubkey_array)
        chain_code = parse_hex_array_from_c(chaincode_array)
        key_hash = parse_hex_array_from_c(keyhash_array)

        entries.append({
            'description': path_desc,
            'path': bip32_path,
            'public_key': public_key,
            'chain_code': chain_code,
            'key_hash': key_hash,
        })

    return entries


def test_all_mock_key_derivation(backend: BackendInterface) -> None:
    """
    Verify that ALL mock path entries match key derivation from test mnemonic.

    This test:
    1. Parses crypto_mock_data.h to extract all mock entries
    2. For each entry, derives the key from the standard mnemonic
    3. Verifies public key, chain code, and key hash match
    """

    mock_entries = parse_mock_paths_from_header()

    if not mock_entries:
        pytest.skip("No mock entries found in crypto_mock_data.h")

    print(f"\nVerifying {len(mock_entries)} mock path entries...")

    for entry in mock_entries:
        path = entry['path']
        description = entry['description']
        expected_pubkey = entry['public_key']
        expected_chaincode = entry['chain_code']
        expected_keyhash = entry['key_hash']

        # Derive the public key using ragger with the standard test mnemonic
        # CRITICAL: Must pass mnemonic parameter
        # If this fails, the test MUST fail - no exceptions!
        derived_pk_hex, derived_chain_code_hex = calculate_public_key_and_chaincode(
            CurveChoice.Ed25519Kholaw,
            path,
            mnemonic=MNEMONIC
        )

        # Remove the "00" prefix from the public key (ragger adds this)
        derived_pk = bytes.fromhex(derived_pk_hex[2:])
        derived_chaincode = bytes.fromhex(derived_chain_code_hex)

        # Verify public key - this MUST match or test fails
        assert derived_pk == expected_pubkey, \
            f"{description} ({path}): Public key mismatch!\n" \
            f"  Expected: {expected_pubkey.hex()}\n" \
            f"  Derived:  {derived_pk.hex()}\n" \
            f"  Check unit-tests/mocks/crypto_mock_data.h"

        # Verify chain code - this MUST match or test fails
        assert derived_chaincode == expected_chaincode, \
            f"{description} ({path}): Chain code mismatch!\n" \
            f"  Expected: {expected_chaincode.hex()}\n" \
            f"  Derived:  {derived_chaincode.hex()}\n" \
            f"  Check unit-tests/mocks/crypto_mock_data.h"

        # Calculate and verify blake2b-224 key hash - this MUST match or test fails
        calculated_keyhash = hashlib.blake2b(derived_pk, digest_size=28).digest()
        assert calculated_keyhash == expected_keyhash, \
            f"{description} ({path}): Key hash mismatch!\n" \
            f"  Expected: {expected_keyhash.hex()}\n" \
            f"  Calculated: {calculated_keyhash.hex()}\n" \
            f"  The key_hash field should be blake2b-224 of the public key"

        print(f"✓ {description} ({path})")

    print(f"\n✓ All {len(mock_entries)} entries verified successfully!")
