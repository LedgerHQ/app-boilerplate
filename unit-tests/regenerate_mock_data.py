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
ragger_configuration = None
try:
    from ragger.bip import calculate_public_key_and_chaincode, CurveChoice
    from ragger.conftest import configuration as ragger_configuration
    from bip_utils import Bip39SeedGenerator, Bip32Ed25519Kholaw
    from nacl import bindings
except ImportError as exc:
    print(f"ERROR: missing dependency: {exc}")
    print("Please activate the venv: source ../tests/standalone/venv/bin/activate")
    print("Or run from within the venv")
    sys.exit(1)

DEFAULT_MNEMONIC = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"

def _resolve_mnemonic() -> str:
    if ragger_configuration is not None:
        optional_seed = getattr(ragger_configuration.OPTIONAL, "CUSTOM_SEED", "")
        if optional_seed:
            return optional_seed
    return DEFAULT_MNEMONIC

MNEMONIC = _resolve_mnemonic()


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

    # Build message lookup table for signature regeneration
    message_pattern = r'static const uint8_t (\w+)\[\] = \{([^}]+)\};'
    messages = {}
    for match in re.finditer(message_pattern, new_content, flags=re.DOTALL):
        name = match.group(1)
        hex_values = re.findall(r'0x[0-9a-fA-F]{2}', match.group(2))
        if not hex_values:
            continue
        messages[name] = bytes(int(value, 16) for value in hex_values)

    seed = Bip39SeedGenerator(MNEMONIC).Generate()

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

    # Regenerate signature entries to match standard mnemonic
    signature_pattern = (
        r'(/\* Path "([^"]+)" message ([^*]+?) \*/\s*'
        r'\{ \.path = (\{[^}]+\}), \.path_len = [^,]+,\s*'
        r'\.message = ([^,]+),\s*\.message_len = [^,]+,\s*'
        r'/\* Signature \(hex\): "([^"]*)" \*/\s*'
        r'\.signature = \{([^}]+)\},\s*'
        r'\},)'
    )

    def regenerate_signature_entry(match):
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
            r'\.signature = \{[^}]+\}',
            f'.signature = {{{format_c_array(signature)}}}',
            entry_text
        )
        return entry_text

    new_content = re.sub(signature_pattern, regenerate_signature_entry, new_content, flags=re.DOTALL)

    # Write output
    with open(output_file, 'w') as f:
        f.write(new_content)

    print(f"\n✓ Regenerated mock data written to: {output_file}")
    print("Review the changes and then:")
    print(f"  mv {output_file} {input_file}")


if __name__ == '__main__':
    main()
