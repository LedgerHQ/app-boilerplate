#!/usr/bin/env python3
"""
Regenerate ALL mock data in crypto_mock_data.h from the standard test mnemonic.

This ensures all public keys, chain codes, and key hashes are correct and consistent
with the derivation from: "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"
"""

import sys
import re
import hashlib
from pathlib import Path

# Try to import ragger from venv
# Note: This script should be run with the tests/standalone/venv activated
# Or ragger should be installed in the current Python environment
try:
    from ragger.bip import calculate_public_key_and_chaincode, CurveChoice
except ImportError:
    print("ERROR: ragger not found!")
    print("Please activate the venv: source ../tests/standalone/venv/bin/activate")
    print("Or run from within the venv")
    sys.exit(1)

MNEMONIC = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"


def parse_bip32_path_from_c_array(path_array_str: str) -> str:
    """Convert C array to BIP32 path string."""
    hex_values = re.findall(r'0x[0-9a-fA-F]+', path_array_str)
    path_parts = ["m"]
    for hex_val in hex_values:
        val = int(hex_val, 16)
        if val & 0x80000000:
            path_parts.append(f"{val & 0x7FFFFFFF}'")
        else:
            path_parts.append(str(val))
    return "/".join(path_parts)


def format_c_array(data: bytes, indent: str = "      ") -> str:
    """Format bytes as C array."""
    chunks = [data[i:i+8] for i in range(0, len(data), 8)]
    lines = []
    for i, chunk in enumerate(chunks):
        prefix = "" if i == 0 else indent + " "
        lines.append(prefix + "0x" + ", 0x".join(f"{b:02x}" for b in chunk))
    return (",\n" + indent).join(lines)


def main():
    input_file = Path(__file__).parent / 'mocks' / 'crypto_mock_data.h'
    output_file = Path(__file__).parent / 'mocks' / 'crypto_mock_data_regenerated.h'

    with open(input_file, 'r') as f:
        content = f.read()

    print("Regenerating mock data from standard test mnemonic...")
    print(f"Input:  {input_file}")
    print(f"Output: {output_file}\n")

    # Process each mock path entry
    def regenerate_entry(match):
        path_desc = match.group(2)  # Fixed: group 2 is the path description string
        path_array = match.group(3)  # Fixed: group 3 is the path array

        # Parse the BIP32 path
        bip32_path = parse_bip32_path_from_c_array(path_array)

        # Derive from standard mnemonic
        try:
            derived_pk_hex, derived_cc_hex = calculate_public_key_and_chaincode(
                CurveChoice.Ed25519Kholaw,
                bip32_path,
                mnemonic=MNEMONIC
            )

            derived_pk = bytes.fromhex(derived_pk_hex[2:])  # Remove "00" prefix
            derived_cc = bytes.fromhex(derived_cc_hex)
            derived_kh = hashlib.blake2b(derived_pk, digest_size=28).digest()

            print(f"✓ {path_desc} ({bip32_path})")

            # Rebuild the entry with correct values
            # Keep the path and private_key as-is, update public_key, chain_code, key_hash
            entry_text = match.group(0)

            # Update public key comment
            entry_text = re.sub(
                r'/\* Public key \(hex\): "[^"]*" \*/',
                f'/* Public key (hex): "{derived_pk.hex()}" */',
                entry_text
            )

            # Update public key array
            entry_text = re.sub(
                r'\.public_key = \{[^}]+\}',
                f'.public_key = {{{format_c_array(derived_pk)}}}',
                entry_text
            )

            # Update chain code comment
            entry_text = re.sub(
                r'/\* Chain code \(hex\): "[^"]*" \*/',
                f'/* Chain code (hex): "{derived_cc.hex()}" */',
                entry_text
            )

            # Update chain code array
            entry_text = re.sub(
                r'\.chain_code = \{[^}]+\}',
                f'.chain_code = {{{format_c_array(derived_cc)}}}',
                entry_text
            )

            # Update key hash comment
            entry_text = re.sub(
                r'/\* Blake2b-224 key hash: [a-f0-9]+ \*/',
                f'/* Blake2b-224 key hash: {derived_kh.hex()} */',
                entry_text
            )

            # Update key hash array
            entry_text = re.sub(
                r'\.key_hash = \{[^}]+\}',
                f'.key_hash = {{{format_c_array(derived_kh)}}}',
                entry_text
            )

            return entry_text

        except Exception as e:
            print(f"✗ {path_desc} ({bip32_path}): {e}")
            return match.group(0)  # Return unchanged on error

    # Pattern to match entire mock path entries
    pattern = r'(/\* Path "([^"]+)"[^{]+\{ \.path = (\{[^}]+\}).*?\.key_hash = \{[^}]+\},\n    \},)'

    new_content = re.sub(pattern, regenerate_entry, content, flags=re.DOTALL)

    # Write output
    with open(output_file, 'w') as f:
        f.write(new_content)

    print(f"\n✓ Regenerated mock data written to: {output_file}")
    print("Review the changes and then:")
    print(f"  mv {output_file} {input_file}")


if __name__ == '__main__':
    main()
