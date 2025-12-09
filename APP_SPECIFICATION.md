# Technical Specification

> **Warning**
This documentation is a template and shall be updated with your own APDUs.

## About

This documentation describes the APDU messages interface to communicate with the Boilerplate application.

The application covers the following functionalities :

- Get a public Boilerplate address given a BIP 32 path
- Sign a basic Boilerplate transaction given a BIP 32 path and raw transaction
- Sign a token transaction given a BIP 32 path, token address, and raw transaction
- Provide dynamic token metadata via CAL (Crypto Asset List) signed descriptors
- Retrieve the Boilerplate app version
- Retrieve the Boilerplate app name

The application interface can be accessed over HID or BLE

## APDUs

### GET BOILERPLATE PUBLIC ADDRESS

#### Description

This command returns the public key for the given BIP 32 path.

The address can be optionally checked on the device before being returned.

#### Coding

##### `Command`

| CLA | INS   | P1                                                 | P2    | Lc       | Le       |
| --- | ---   | ---                                                | ---   | ---      | ---      |
| E0  |  05   |  00 : return address                               | 00    | variable | variable |
|     |       |  01 : display address and confirm before returning |       |          |          |

##### `Input data`

| Description                                                      | Length |
| ---                                                              | ---    |
| Number of BIP 32 derivations to perform (max 10 levels)          | 1      |
| First derivation index (big endian)                              | 4      |
| ...                                                              | 4      |
| Last derivation index (big endian)                               | 4      |

##### `Output data`

| Description                                                      | Length |
| ---                                                              | ---    |
| Public Key length                                                | 1      |
| Public Key                                                       | var    |
| Chain code length                                                | 1      |
| Chain code                                                       | var    |

### SIGN BOILERPLATE TRANSACTION

#### Description

This command signs a Boilerplate transaction after having the user validate the transactions parameters.

The input data is the RLP encoded transaction streamed to the device in 255 bytes maximum data chunks.

#### Coding

##### `Command`

| CLA | INS  | P1           | P2           | Lc       | Le       |
| --- | ---  | ---          | ---          | ---      | ---      |
| E0  | 06   | Chunk index  | More chunks  | variable | variable |

**P1 - Chunk index:**

- `0x00`: First chunk (contains BIP32 path)
- `0x01`: Second chunk
- `0x02`: Third chunk
- ...
- `0xFF`: Maximum chunk index

**P2 - More chunks flag:**

- `0x00`: Last chunk (no more data)
- `0x80`: More chunks to follow

##### `Examples`

###### Example 1: Single APDU (small transaction)

When the entire transaction (including BIP32 path) fits in one APDU:

```console
P1:   00  (first chunk)
P2:   00  (last chunk, no more data)
```

###### Example 2: Three APDUs (large transaction)

When transaction data requires chunking across multiple APDUs:

*First APDU (P1=0x00, P2=0x80):*
*Second APDU (P1=0x01, P2=0x80):*
*Third APDU (P1=0x02, P2=0x00):*

##### `Input data (first transaction data block)`

| Description                                               | Length   |
| ---                                                       | ---      |
| Number of BIP 32 derivations to perform (max 10 levels)   | 1        |
| First derivation index (big endian)                       | 4        |
| ...                                                       | 4        |
| Last derivation index (big endian)                        | 4        |

##### `Input data (other transaction data block)`

| Description                                          | Length   |
| ---                                                  | ---      |
| Transaction chunk                                    | variable |

##### `Output data`

| Description                                          | Length   |
| ---                                                  | ---      |
| Signature length                                     | 1        |
| Signature                                            | variable |
| v                                                    | 1        |

### SIGN BOILERPLATE TOKEN TRANSACTION

#### Description

This command signs a mock token transaction after user on-screen validation.

The application will only display and sign transaction for tokens it has knowledge about (address/ticker/magnitude).
Information is fetched from the internal hardcoded token database, or received from the PROVIDE_TOKEN_INFO API.

The tokens and the token transaction format are made up for showcase purposes.

#### Coding

##### `Command`

| CLA | INS  | P1           | P2           | Lc       | Le       |
| --- | ---  | ---          | ---          | ---      | ---      |
| E0  | 07   | Chunk index  | More chunks  | variable | variable |

**P1 - Chunk index:**

- `0x00`: First chunk (contains BIP32 path)
- `0x01`: Second chunk
- `0x02`: Third chunk
- ...
- `0xFF`: Maximum chunk index

**P2 - More chunks flag:**

- `0x00`: Last chunk (no more data)
- `0x80`: More chunks to follow

##### `Examples`

###### Example 1: Single APDU (small transaction)

When the entire transaction (including BIP32 path) fits in one APDU:

```console
P1:   00  (first chunk)
P2:   00  (last chunk, no more data)
```

###### Example 2: Three APDUs (large transaction)

When transaction data requires chunking across multiple APDUs:

*First APDU (P1=0x00, P2=0x80):*
*Second APDU (P1=0x01, P2=0x80):*
*Third APDU (P1=0x02, P2=0x00):*

##### `Input data (first transaction data block)`

| Description                                               | Length   |
| ---                                                       | ---      |
| Number of BIP 32 derivations to perform (max 10 levels)   | 1        |
| First derivation index (big endian)                       | 4        |
| ...                                                       | 4        |
| Last derivation index (big endian)                        | 4        |

##### `Input data (other transaction data block)`

| Description                                          | Length   |
| ---                                                  | ---      |
| Token transaction chunk (includes 32-byte token address) | variable |

##### `Transaction format`

The token transaction has the following format:

- Nonce (8 bytes, big endian)
- To address (20 bytes)
- Token address (32 bytes) - must be in the token database
- Amount (8 bytes, big endian) - in token's smallest unit
- Memo length (varint)
- Memo (variable length)

##### `Output data`

| Description                                          | Length   |
| ---                                                  | ---      |
| Signature length                                     | 1        |
| Signature                                            | variable |
| v                                                    | 1        |

### GET APP VERSION

#### Description

This command returns boilerplate application version

#### Coding

##### `Command`

| CLA | INS | P1  | P2  | Lc   | Le |
| --- | --- | --- | --- | ---  | ---|
| E0  | 03  | 00  | 00  | 00   | 04 |

##### `Input data`

None

##### `Output data`

| Description                       | Length |
| ---                               | ---    |
| Application major version         | 01 |
| Application minor version         | 01 |
| Application patch version         | 01 |

### GET APP NAME

#### Description

This command returns boilerplate application name

#### Coding

##### `Command`

| CLA | INS | P1  | P2  | Lc   | Le |
| --- | --- | --- | --- | ---  | ---|
| E0  | 04  | 00  | 00  | 00   | 04 |

##### `Input data`

None

##### `Output data`

| Description           | Length   |
| ---                   | ---      |
| Application name      | variable |

### PROVIDE TOKEN INFO

#### Description

This command provides dynamic token metadata to the device via a CAL (Crypto Asset List) signed descriptor.
The descriptor is a TLV-encoded message signed by the CAL (a Ledger HSM) and the signature
is validated using the device's PKI certificate infrastructure.

Once provided, the dynamic token information takes priority over the hardcoded token database
for subsequent token transaction signing. The token metadata persists in RAM across
commands (including regular transactions) until the app exits or a new token is provided.

This enables tokens to be added without firmware updates.

**IMPORTANT FOR THIRD-PARTY DEVELOPERS:**

- For this feature to work on a given application, the CAL needs to maintain the knowledge
  of the relevant tokens. This feature thus **requires coordination with Ledger teams**
  before implementation. Please reach out before implementing it.
- The hardcoded token database is a simpler token management method as it only involves the application.

Security model

- The CAL key must first be whitelisted with correct permissions on the device PKI,
  this is done by sending a certificate with the OS APDU header (starting by 0xB006)
- We can then send the TLV signed with the CAL key, the os_pki_verify lib call will
  ensure the TLV is signed by a whitelisted authority (the onboarded CAL key).
- In the test framework, we created a local fake CAL key and crafted a certificate
  with TEST permissions. It be accepted by Speculos but not by a real device.

#### Token Lookup Priority

Once a dynamic token is provided via `PROVIDE_TOKEN_INFO`, the token database lookup in `get_token_info()` follows this priority:

1. **Dynamic token (CAL)**: Check a dynamic token has been received
2. **Hardcoded database**: Check built-in token database
3. **Unknown**: Refuses to sign. A fallback method could be implemented instead (Display token address, blind sign, ...)

This means CAL-provided tokens **override** hardcoded database entries with the same address.

#### Persistence Behavior

The dynamic token information is stored in RAM only (not in NVM) and persists across commands:

- Persists across `SIGN_TX` (regular transactions)
- Persists across `SIGN_TOKEN_TX` (token transactions)
- Persists across `GET_PUBLIC_KEY` and other read-only commands
- Cleared when app exits to dashboard
- Cleared after every transaction in SWAP context
- Overwritten when new `PROVIDE_TOKEN_INFO` command received (only one slot is handled)

#### Coding

##### `Command`

| CLA | INS  | P1  | P2  | Lc       | Le       |
| --- | ---  | --- | --- | ---      | ---      |
| E0  | 22   | 00  | 00  | variable | 00       |

##### `Input data`

The input data is a TLV-encoded descriptor with the following outer tags
(all tags required, order matters for signature verification):

| Tag  | Name           | Length  | Description                                       |
| ---  | ---            | ---     | ---                                               |
| 0x01 | STRUCTURE_TYPE | 1 byte  | Descriptor type (DYNAMIC_TOKEN = 0x90)            |
| 0x02 | VERSION        | 1 byte  | Version of the serialization format               |
| 0x03 | COIN_TYPE      |4 bytes  | SLIP-44 coin type                                 |
| 0x04 | APP_NAME       | var     | Name of the AppCoin. Case sensitive               |
| 0x05 | TICKER         | var     | Token ticker that will be displayed on the device |
| 0x06 | MAGNITUDE      | 1 byte  | Decimals to format the amount in Ticker           |
| 0x07 | TUID           | var     | Token Unique Identifier (app specific)            |
| 0x08 | SIGNATURE      | [70-72] | Signature of above fields                         |

**TUID Field (tag 0x07):**
The TUID field content is application specific. It is recommended to make it a nested TLV structure itself.
In the Boilerplate made up implementation it contains a single tag:

| Tag  | Description        | Length |
| ---  | ---                | ---    |
| 0x10 | Token address      | 32 bytes |

**Signature construction:**
The signature (tag 0x08) is computed over all tags EXCEPT tag 0x08 itself.
In production context, the CAL is responsible for this signature.
In our test framework, we use a mock CAL to dynamically craft signatures.

##### `Output data`

None (returns 0x9000 on success)

##### `Example TLV structure`

```console
01 01 90                    # STRUCTURE_TYPE = DYNAMIC_TOKEN (0x90)
02 01 01                    # VERSION = 1
03 04 80 00 80 01           # CHAIN_ID = 0x80008001 (hardened 0x8001)
04 01 00                    # SIGNER_ALGO = SECP256K1 (0x00)
05 01 08                    # SIGNER_KEY = COIN_META (0x08)
06 40 <64 bytes signature>  # DER_SIGNATURE (r || s)
07 22                       # TUID length (34 bytes including sub-TLV)
   10 20 <32 bytes>         #   Sub-tag 0x10: token address
08 07                       # APP_DATA length (7 bytes)
   04 55534454              #   Ticker length (4) + "USDT"
   06                       #   Decimals (6)
```

##### `Errors`

| SW   | Description                                           |
| ---  | ---                                                   |
| B009 | SW_INVALID_DYNAMIC_TOKEN - TLV parsing failed, signature verification failed, wrong coin type, or TUID validation failed |
| 6A86 | SWO_INCORRECT_P1_P2 - P1 or P2 not zero               |
| 6A87 | SWO_WRONG_DATA_LENGTH - Invalid TLV structure length  |

**Note on testing:** Speculos emulator accepts test PKI certificates for signature validation,
but real Ledger devices reject them. This is a OS security feature independent of application code or build flags.

## Status Words

The following standard Status Words are returned for all APDUs.

| SW       | SW name                      | Description                                 |
| ---      | ---                          | ---                                         |
|   6985   | SWO_CONDITIONS_NOT_SATISFIED | Rejected by user                            |
|   6A86   | SWO_INCORRECT_P1_P2          | Either P1 or P2 is incorrect                |
|   6A87   | SWO_WRONG_DATA_LENGTH        | Lc or minimum APDU length is incorrect      |
|   6D00   | SWO_INVALID_INS              | No command exists with INS                  |
|   6E00   | SWO_INVALID_CLA              | Bad CLA used for this application           |
|   6A80   | SWO_INCORRECT_DATA           | Failed to parse raw transaction             |
|   6985   | SWO_CONDITIONS_NOT_SATISFIED | Security issue with bad state               |
|   6600   | SWO_SECURITY_ISSUE           | Signature of raw transaction failed         |
|   B009   | SW_INVALID_DYNAMIC_TOKEN     | Dynamic token TLV parsing/validation failed |
|   9000   | OK                           | Success                                     |
