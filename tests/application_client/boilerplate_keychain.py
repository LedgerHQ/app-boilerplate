"""
Keychain management for CAL dynamic token PKI signatures.

This module provides SECP256K1 signing infrastructure for generating
test signatures that validate dynamic token descriptors via the CAL
(Crypto Asset List) public key infrastructure.

The signing key (keychain/dynamic_token.pem) MUST match the key sent in the PKI
test certificate. If you regenerate it you will not be able to generate the
corresponding test PKI certificate.
"""

import os
import hashlib
from enum import Enum, auto
from ecdsa import SigningKey  # type: ignore[import-untyped]
from ecdsa.util import sigencode_der  # type: ignore[import-untyped]


class Key(Enum):
    """Available signing keys for CAL dynamic token descriptors."""

    DYNAMIC_TOKEN = auto()


def _load_key(key: Key) -> SigningKey:
    """Load signing key from PEM file."""
    keychain_path = os.path.join(
        os.path.dirname(__file__), "keychain", f"{key.name.lower()}.pem"
    )

    if not os.path.exists(keychain_path):
        raise FileNotFoundError(
            f"Keychain file not found: {keychain_path}\n"
            f"This key should be copied from Solana app\n"
            f"DO NOT generate a new key - must match Solana reference."
        )

    with open(keychain_path, encoding="utf-8") as pem_file:
        return SigningKey.from_pem(pem_file.read(), hashlib.sha256)


def sign_data(data: bytes, key: Key = Key.DYNAMIC_TOKEN) -> bytes:
    """
    Generate a SECP256K1 signature of the given data with the given key.

    Args:
        data: Data to sign (typically the complete TLV descriptor without signature)
        key: Which key to use from the keychain

    Returns:
        DER-encoded signature
    """
    signing_key = _load_key(key)
    return signing_key.sign_deterministic(data, sigencode=sigencode_der)
