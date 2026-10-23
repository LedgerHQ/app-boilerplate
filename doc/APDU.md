# APDU Reference

> **Warning**
This documentation is a template and shall be updated with your own APDUs.

This document is the **static** reference of the Boilerplate application interface: the encoding of every
command, its parameters, its response and its status words, and the format of the transactions it signs.

How the commands are chained together to achieve a feature (address verification, transaction signing,
dynamic token, swap) is described in the **dynamic** companion document: [FLOWS.md](FLOWS.md).

- [Transport](#transport)
- [Command summary](#command-summary)
- [Common encodings](#common-encodings)
- [Transaction format](#transaction-format)
- [Commands](#commands)
- [OS commands used by the host](#os-commands-used-by-the-host)
- [Status words](#status-words)

## Transport

The application interface can be accessed over USB HID or BLE.

The communication protocol used by [BOLOS](https://ledger.readthedocs.io/en/latest/bolos/overview.html)
to exchange [APDU](https://en.wikipedia.org/wiki/Smart_card_application_protocol_data_unit)
is very close to [ISO 7816-4](https://www.iso.org/standard/77180.html) with a few differences:

- `Lc` length is always exactly 1 byte
- No `Le` field in APDU command
- Maximum size of APDU command is 260 bytes: 5 bytes of header + 255 bytes of data
- Maximum size of APDU response is 260 bytes: 258 bytes of response data + 2 bytes of status word

### Command APDU

| Field name | Length (bytes) | Description |
| --- | --- | --- |
| CLA | 1 | Instruction class, `0xE0` for every Boilerplate command |
| INS | 1 | Instruction code, indicates the specific command |
| P1 | 1 | Instruction parameter 1 for the command |
| P2 | 1 | Instruction parameter 2 for the command |
| Lc | 1 | The number of bytes of command data to follow (a value from 0 to 255) |
| CData | var | Command data with `Lc` bytes |

### Response APDU

| Field name | Length (bytes) | Description |
| --- | --- | --- |
| RData | var | Response data (can be empty) |
| SW | 2 | Status word containing command processing status (e.g. `0x9000` for success) |

## Command summary

| INS | Command | P1 | P2 | User interaction | Section |
| --- | --- | --- | --- | --- | --- |
| `0x03` | GET_VERSION | `0x00` | `0x00` | None | [GET_VERSION](#get_version) |
| `0x04` | GET_APP_NAME | `0x00` | `0x00` | None | [GET_APP_NAME](#get_app_name) |
| `0x05` | GET_PUBLIC_KEY | `0x00` silent, `0x01` display | `0x00` | Optional address review | [GET_PUBLIC_KEY](#get_public_key) |
| `0x06` | SIGN_TX | Chunk index | More chunks flag | Transaction review | [SIGN_TX](#sign_tx) |
| `0x07` | SIGN_TOKEN_TX | Chunk index | More chunks flag | Token transaction review | [SIGN_TOKEN_TX](#sign_token_tx) |
| `0x22` | PROVIDE_TOKEN_INFO | `0x00` | `0x00` | None | [PROVIDE_TOKEN_INFO](#provide_token_info) |

Any other INS is rejected with `0x6D00`, and any CLA other than `0xE0` with `0x6E00`.

## Common encodings

### BIP32 path

| Description | Length (bytes) |
| --- | --- |
| Number of BIP32 derivations to perform (max 10) | 1 |
| First derivation index (big endian) | 4 |
| ... | 4 |
| Last derivation index (big endian) | 4 |

Example, `m/44'/1'/0'/0/0`:

```text
05 8000002c 80000001 80000000 00000000 00000000
```

### Chunked commands

`SIGN_TX` and `SIGN_TOKEN_TX` carry more data than a single APDU can hold, so they are split in chunks:

| Chunk | P1 | P2 | Data |
| --- | --- | --- | --- |
| First | `0x00` | `0x80` (mandatory) | [BIP32 path](#bip32-path) only |
| Intermediate | `0x01`, `0x02` | `0x80` | Next part of the serialized transaction (max 255 bytes) |
| Last | `0x01` to `0x03` | `0x00` | Last part of the serialized transaction |

Constraints enforced by the application:

- The first chunk always carries the BIP32 path alone, and must have `P2 = 0x80`: a transaction always takes
  at least 2 APDUs. A first chunk with `P2 = 0x00` is rejected with `0x6A86`.
- `P1` above `0x03` (`P1_MAX`) is rejected with `0x6A86`: at most 3 transaction chunks follow the path chunk.
- The serialized transaction is limited to 510 bytes (`MAX_TRANSACTION_LEN`), longer data is rejected with
  `0x6A87`.
- All chunks of a transaction must use the same INS as the first one, otherwise `0x6985` is returned.
- A new first chunk (`P1 = 0x00`) always resets the signing context.

The application does not check that `P1` increments by one between chunks: the host is expected to send
`0x01`, `0x02`, `0x03` in order.

### Variable length integer (varint)

Integer can be encoded depending on the represented value to save space. Variable length integers always
precede an array of a type of data that may vary in length. Longer numbers are encoded in little endian.

| Value | Storage length (bytes) | Format |
| --- | :---: | --- |
| <= 0xFC | 1 | uint8_t |
| <= 0xFFFF | 3 | 0xFD followed by the value as uint16_t |
| <= 0xFFFF FFFF | 5 | 0xFE followed by the value as uint32_t |
| - | 9 | 0xFF followed by the value as uint64_t |

## Transaction format

The transactions signed by `SIGN_TX` and `SIGN_TOKEN_TX` follow the serialization of the purely fictitious
BOLOK *chain*, inspired by other popular blockchains
([Bitcoin](https://en.bitcoin.it/wiki/Protocol_documentation#tx),
[Ethereum](https://ethereum.github.io/yellowpaper/paper.pdf#subsection.4.2)).

**Note:** this format is for demonstration purposes only. It is not intended for actual blockchain use and
should not be deployed in production environments.

### Amount units

The base unit in BOLOK *chain* is the BOL and the smallest unit used in raw transaction is
the *bolino* or mBOL: 1 BOL = 1000 mBOL. Token amounts are expressed in the smallest unit of the token, and
displayed with the decimals of that token.

### Address format

BOLOK addresses are the last 20 bytes of the Keccak-256 hash of the public key, without its `04` prefix
(x and y coordinates, 64 bytes). They are displayed as hexadecimal.

### Regular transaction

Sent with [SIGN_TX](#sign_tx), after the BIP32 path chunk:

| Field | Size (bytes) | Description |
| --- | :---: | --- |
| `nonce` | 8 | A sequence number used to prevent message replay, big endian |
| `to` | 20 | The destination address |
| `value` | 8 | The amount in mBOL to send to the destination address, big endian |
| `memo_len` | 1, 3, 5 or 9 | Length of the memo as [varint](#variable-length-integer-varint) |
| `memo` | var | An ASCII text of length `memo_len` (bytes up to `0x7F` only) |

### Token transaction

Sent with [SIGN_TOKEN_TX](#sign_token_tx), after the BIP32 path chunk:

| Field | Size (bytes) | Description |
| --- | :---: | --- |
| `nonce` | 8 | A sequence number used to prevent message replay, big endian |
| `to` | 20 | The destination address |
| `token_address` | 32 | The token address, known by the hardcoded database or provided with [PROVIDE_TOKEN_INFO](#provide_token_info) |
| `value` | 8 | The amount in the smallest unit of the token, big endian |
| `memo_len` | 1, 3, 5 or 9 | Length of the memo as [varint](#variable-length-integer-varint) |
| `memo` | var | An ASCII text of length `memo_len` (bytes up to `0x7F` only) |

### Parsing rules

- The whole serialized transaction is limited to 510 bytes.
- The transaction must end exactly after the memo: trailing bytes are rejected with `0x6A80`.
- There is no fee field: the BOLOK *chain* has constant fees.

### Signature

The signed message is the Keccak-256 hash of the whole serialized transaction, as received in the chunks
(including `token_address` for a token transaction). It is signed with deterministic ECDSA
([RFC 6979](https://tools.ietf.org/html/rfc6979)) on the
[secp256k1](https://www.secg.org/sec2-v2.pdf#subsubsection.2.4.1) curve.

The signature is not part of the transaction sent to the device: it is returned in the response of the last
chunk, see [SIGN_TX](#sign_tx).

## Commands

### GET_VERSION

Returns the application version.

#### Command

| CLA | INS | P1 | P2 | Lc |
| --- | --- | --- | --- | --- |
| `E0` | `03` | `00` | `00` | `00` |

#### Response

| Description | Length (bytes) |
| --- | --- |
| Major version | 1 |
| Minor version | 1 |
| Patch version | 1 |

#### Errors

| SW | Condition |
| --- | --- |
| `6A86` | P1 or P2 is not `0x00` |

### GET_APP_NAME

Returns the application name (`APPNAME` in the Makefile), as ASCII, without length prefix.

#### Command

| CLA | INS | P1 | P2 | Lc |
| --- | --- | --- | --- | --- |
| `E0` | `04` | `00` | `00` | `00` |

#### Response

| Description | Length (bytes) |
| --- | --- |
| Application name | var (max 64) |

#### Errors

| SW | Condition |
| --- | --- |
| `6A86` | P1 or P2 is not `0x00` |

### GET_PUBLIC_KEY

Returns the uncompressed public key and the chain code for the given BIP32 path. With `P1 = 0x01`, the
address derived from the public key is displayed and the response is only sent once the user approves it.

#### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| `E0` | `05` | `00`: return the public key<br>`01`: display the address and confirm first | `00` | var | [BIP32 path](#bip32-path) |

#### Response

| Description | Length (bytes) |
| --- | --- |
| Public key length (`0x41`) | 1 |
| Public key (`04`, then x and y coordinates) | 65 |
| Chain code length (`0x20`) | 1 |
| Chain code | 32 |

#### Errors

| SW | Condition |
| --- | --- |
| `6A86` | P1 > `0x01` or P2 is not `0x00` |
| `6A87` | Missing data or malformed BIP32 path |
| `6985` | Address rejected by the user (`P1 = 0x01` only) |

### SIGN_TX

Signs a native BOL transaction after the user reviewed and approved it. The transaction is sent in
[chunks](#chunked-commands), and its serialization is described in
[Regular transaction](#regular-transaction).

The signed message is `Keccak-256(serialized transaction)`, signed with deterministic ECDSA (RFC 6979) on
secp256k1.

#### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| `E0` | `06` | `00`: first chunk | `80` | var | [BIP32 path](#bip32-path) |
| `E0` | `06` | `01` to `03`: chunk index | `80`: more chunks<br>`00`: last chunk | var | Transaction chunk |

#### Response

Every chunk except the last one returns an empty response with `0x9000`. The last chunk triggers the review
and returns, once approved:

| Description | Length (bytes) |
| --- | --- |
| Signature length | 1 |
| Signature (DER encoded) | var (max 72) |
| `v`: parity of the y-coordinate of R (`0x00` even, `0x01` odd) | 1 |

#### Errors

| SW | Condition |
| --- | --- |
| `6A86` | Invalid P1/P2 combination, see [chunked commands](#chunked-commands) |
| `6A87` | Missing data, malformed BIP32 path or transaction longer than 510 bytes |
| `6985` | Chunk received without a prior first chunk for the same INS, or transaction rejected by the user |
| `6A80` | Transaction cannot be parsed |
| `6600` | Signature computation failed |

### SIGN_TOKEN_TX

Same as [SIGN_TX](#sign_tx), for a token transaction. The serialization, which adds a 32-byte token address,
is described in [Token transaction](#token-transaction).

The token must be known by the application before the last chunk is sent, either from the hardcoded token
database or from a descriptor previously sent with [PROVIDE_TOKEN_INFO](#provide_token_info). The amount is
displayed with the ticker and the decimals of that token.

#### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| `E0` | `07` | `00`: first chunk | `80` | var | [BIP32 path](#bip32-path) |
| `E0` | `07` | `01` to `03`: chunk index | `80`: more chunks<br>`00`: last chunk | var | Token transaction chunk |

#### Response

Same as [SIGN_TX](#sign_tx).

#### Errors

Same as [SIGN_TX](#sign_tx), plus:

| SW | Condition |
| --- | --- |
| `6A80` | Token address unknown (neither in the hardcoded database nor provided dynamically) |

### PROVIDE_TOKEN_INFO

Provides the metadata of one token (ticker, decimals) in a TLV descriptor signed by the Ledger CAL
(Crypto Asset List). The signature is verified against the CAL key, which must have been loaded beforehand
with the OS command [LOAD_CERTIFICATE](#load_certificate) and the `COIN_META` (`0x08`) key usage.

**Third-party developers:** this feature requires the CAL to know your tokens, and thus coordination with
Ledger teams. The hardcoded token database (`src/token/token_db.c`) only involves the application.

#### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| `E0` | `22` | `00` | `00` | var | TLV descriptor |

#### Descriptor

Each field is encoded as `tag (1 byte) || length (1 byte) || value`. All tags are mandatory and must be
unique.

| Tag | Name | Length (bytes) | Description |
| --- | --- | --- | --- |
| `0x01` | STRUCTURE_TYPE | 1 | `0x90` (dynamic token) |
| `0x02` | VERSION | 1 | Version of the descriptor format |
| `0x03` | COIN_TYPE | up to 4 | SLIP-44 coin type, big endian. Must be `0x8001` or `0x80008001` |
| `0x04` | APPLICATION_NAME | var | Name of the application, case sensitive. Must be `Boilerplate` |
| `0x05` | TICKER | var | Ticker displayed on the device |
| `0x06` | MAGNITUDE | 1 | Number of decimals |
| `0x07` | TUID | var | Token Unique IDentifier, application specific, see below |
| `0x08` | SIGNATURE | 70 to 72 | DER signature by the CAL key of all the previous fields |

The TUID is itself a TLV. The Boilerplate application expects exactly one tag, and rejects any other:

| Tag | Name | Length (bytes) | Description |
| --- | --- | --- | --- |
| `0x10` | TOKEN_ADDRESS | 32 | Token address, as found in the token transaction |

Example (ticker `USDT`, 6 decimals):

```text
01 01 90                      STRUCTURE_TYPE = dynamic token
02 01 01                      VERSION = 1
03 02 80 01                   COIN_TYPE = 0x8001
04 0b 426f696c6572706c617465  APPLICATION_NAME = "Boilerplate"
05 04 55534454                TICKER = "USDT"
06 01 06                      MAGNITUDE = 6
07 22                         TUID (34 bytes)
   10 20 <32 bytes>             TOKEN_ADDRESS
08 46 <70 bytes>              SIGNATURE
```

#### Response

Empty, `0x9000` on success.

The token is then stored in RAM, in a single slot, until the application exits:

- it takes priority over a hardcoded token with the same address
- it is kept across all other commands
- it is replaced by the next `PROVIDE_TOKEN_INFO`, and erased if that next descriptor is invalid

#### Errors

| SW | Condition |
| --- | --- |
| `6A86` | P1 or P2 is not `0x00` |
| `6A87` | Missing data |
| `B009` | Invalid descriptor: malformed TLV, missing or duplicated tag, bad signature, unknown CAL key, wrong application name, wrong coin type, or invalid TUID |

## OS commands used by the host

These commands are handled by the OS, not by the application, but are part of the flows described in
[FLOWS.md](FLOWS.md).

### GET_APP_AND_VERSION

| CLA | INS | P1 | P2 | Lc |
| --- | --- | --- | --- | --- |
| `B0` | `01` | `00` | `00` | `00` |

Response: `01`, then the name length and the name, the version length and the version, the flags length and
the flags.

### LOAD_CERTIFICATE

Loads a PKI certificate, here the certificate of the CAL key used to verify
[PROVIDE_TOKEN_INFO](#provide_token_info) descriptors.

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| `B0` | `06` | Key usage, `08` (`COIN_META`) | `00` | var | Certificate |

Speculos accepts the test certificates used by the functional tests, real devices only accept certificates
issued by Ledger.

## Status words

| SW | Name | Description |
| --- | --- | --- |
| `9000` | SWO_SUCCESS | Success |
| `6985` | SWO_CONDITIONS_NOT_SATISFIED | Rejected by the user, or command received in an unexpected state |
| `6A80` | SWO_INCORRECT_DATA | Transaction cannot be parsed, or unknown token |
| `6A86` | SWO_INCORRECT_P1_P2 | Either P1 or P2 is incorrect |
| `6A87` | SWO_WRONG_DATA_LENGTH | Missing data, or data length is incorrect |
| `6D00` | SWO_INVALID_INS | No command exists with INS |
| `6E00` | SWO_INVALID_CLA | Bad CLA used for this application |
| `6600` | SWO_SECURITY_ISSUE | Signature computation failed |
| `B009` | SW_INVALID_DYNAMIC_TOKEN | Dynamic token descriptor parsing or validation failed |
| `C000` | SW_SWAP_FAIL | Transaction refused in swap context, see [FLOWS.md](FLOWS.md#swap) |
