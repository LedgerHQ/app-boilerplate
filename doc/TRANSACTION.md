# BOLOK Transaction Serialization

## Overview

The custom transaction serialization presented is for the purely fictitious BOLOK *chain*
which has been inspired by other popular blockchain (see [Links](#links)).

## Amount units

The base unit in BOLOK *chain* is the BOL and the smallest unit used in raw transaction is
the *bolino* or mBOL: 1 BOL = 1000 mBOL.

## Address format

BOLOK addresses are hexadecimal numbers, identifiers derived from the last 20 bytes of the Keccak-256 hash
of the public key.

## Structure

### Regular Transaction

| Field | Size (bytes) | Description |
| --- | :---: | --- |
| `nonce` | 8 | A sequence number used to prevent message replay |
| `to` | 20 | The destination address |
| `value` | 8 | The amount in mBOL to send to the destination address |
| `memo_len` | 1-9 | length of the memo as [varint](#variable-length-integer-varint) |
| `memo` | var | A text ASCII-encoded of length `memo_len` to show your love |
| `v` | 1 | 0x01 if y-coordinate of R is odd, 0x00 otherwise |
| `r` | 32 | x-coordinate of R in ECDSA signature |
| `s` | 32 | x-coordinate of S in ECDSA signature |

### Token Transaction

| Field | Size (bytes) | Description |
| --- | :---: | --- |
| `nonce` | 8 | A sequence number used to prevent message replay |
| `to` | 20 | The destination address |
| `token_address` | 32 | The 32-byte token contract address (must be in token database) |
| `value` | 8 | The amount in token's smallest unit to send |
| `memo_len` | 1-9 | length of the memo as [varint](#variable-length-integer-varint) |
| `memo` | var | A text ASCII-encoded of length `memo_len` to show your love |
| `v` | 1 | 0x01 if y-coordinate of R is odd, 0x00 otherwise |
| `r` | 32 | x-coordinate of R in ECDSA signature |
| `s` | 32 | x-coordinate of S in ECDSA signature |

**Note**: Token transactions use the SIGN_TOKEN_TX command (INS 0x07) and include a 32-byte token address.
The token must exist in the hardcoded token database or dynamically received for the transaction to be valid.
The value is interpreted using the token's specific decimal places (12 or 14 decimals).

**Note:** This implementation is for demonstration purposes only. It showcases dynamic token TLV (Type-Length-Value)
handling and uses a hardcoded token database. This is not intended for actual blockchain use
and should not be deployed in production environments.

These token transactions are pure examples designed to illustrate:

- How to parse and process token metadata using TLV encoding
- Dynamic handling of token information structures
- Mock token database integration for testing and development

### Variable length integer (varint)

Integer can be encoded depending on the represented value to save space.
Variable length integers always precede an array of a type of data that may vary in length.
Longer numbers are encoded in little endian.

| Value | Storage length (bytes) | Format |
| --- | :---: | --- |
| < 0xFD | 1 | uint8_t |
| <= 0xFFFF | 3 | 0xFD followed by the length as uint16_t |
| <= 0xFFFF FFFF | 5 | 0xFE followed by the length as uint32_t |
| - | 9 | 0xFF followed by the length as uint64_t |

### Signature

Deterministic ECDSA ([RFC 6979](https://tools.ietf.org/html/rfc6979)) is used to sign transaction
on the [SECP-256k1](https://www.secg.org/sec2-v2.pdf#subsubsection.2.4.1) curve.
The signed message is `m = Keccak-256(nonce || to || value || memo_len || memo)`.

### Fee

You won't find any fee in the transaction structure because the BOLOK *chain* has constant fees.

## Links

- [Bitcoin Transaction](https://en.bitcoin.it/wiki/Protocol_documentation#tx)
- [Ethereum Transaction](https://ethereum.github.io/yellowpaper/paper.pdf#subsection.4.2)
