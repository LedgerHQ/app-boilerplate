### ML-DSA KEY GENERATION

#### Description

Generates an ML-DSA keypair from a 32-byte deterministic seed (ξ).

Supports ML-DSA-44 and ML-DSA-65 parameter sets, selected via the P2 byte.
Because ML-DSA keys are larger than a single APDU payload, command data and response data are chunked.

The handler uses the `sign_mu` / `verify_mu` interface: the host pre-computes
`mu = H(tr || M')` and sends it instead of the raw message. This avoids
transmitting the full message to the device.

#### Coding

##### `Command`

| CLA | INS | P1          | P2                  | Lc       |
| --- | --- | ---         | ---                 | ---      |
| E0  | 40  | Chunk index | Param set + More/Last | variable |

**P2 encoding:**

| Bit(s) | Name       | Description                                      |
| ---    | ---        | ---                                              |
| 0-1    | param_set  | `0x00` = ML-DSA-44, `0x01` = ML-DSA-65           |
| 2-6    | reserved   | Must be `0` (validated by the application)        |
| 7      | more       | `0` = last chunk, `1` = more chunks to follow    |

##### `Input data`

| Description                     | Length |
| ---                             | ---    |
| Seed (ξ)                        | 32     |

##### `Output data (ML-DSA-44)`

| Description     | Length |
| ---             | ---    |
| Public key      | 1312   |
| Secret key      | 2560   |

##### `Output data (ML-DSA-65)`

| Description     | Length |
| ---             | ---    |
| Public key      | 1952   |
| Secret key      | 4032   |

### ML-DSA SIGNING

#### Description

Signs a pre-computed mu using the secret key.
The host must compute `mu = H(tr || M')` before calling this command.

#### Coding

##### `Command`

| CLA | INS | P1          | P2                  | Lc       |
| --- | --- | ---         | ---                 | ---      |
| E0  | 41  | Chunk index | Param set + More/Last | variable |

P2 encoding is the same as ML-DSA KEY GENERATION.

##### `Input data (ML-DSA-44)`

| Description     | Length |
| ---             | ---    |
| Secret key (sk) | 2560   |
| mu              | 64     |

##### `Input data (ML-DSA-65)`

| Description     | Length |
| ---             | ---    |
| Secret key (sk) | 4032   |
| mu              | 64     |

##### `Output data (ML-DSA-44)`

| Description     | Length |
| ---             | ---    |
| Signature       | 2420   |

##### `Output data (ML-DSA-65)`

| Description     | Length |
| ---             | ---    |
| Signature       | 3309   |

### ML-DSA VERIFICATION

#### Description

Verifies a signature against a pre-computed mu and public key.
The host must compute `mu = H(tr || M')` before calling this command.

#### Coding

##### `Command`

| CLA | INS | P1          | P2                  | Lc       |
| --- | --- | ---         | ---                 | ---      |
| E0  | 42  | Chunk index | Param set + More/Last | variable |

P2 encoding is the same as ML-DSA KEY GENERATION.

##### `Input data (ML-DSA-44)`

| Description     | Length |
| ---             | ---    |
| Public key (pk) | 1312   |
| Signature       | 2420   |
| mu              | 64     |

##### `Input data (ML-DSA-65)`

| Description     | Length |
| ---             | ---    |
| Public key (pk) | 1952   |
| Signature       | 3309   |
| mu              | 64     |

##### `Output data`

| Description         | Length |
| ---                 | ---    |
| Verification result | 1      |

- `0x00` = signature valid
- `0x01` = signature invalid

### ML-DSA Chunked response framing

For `INS=0x40/0x41/0x42`, the same chunked response framing as ML-KEM applies:

- Final input APDU response contains the first output chunk
- First output chunk format: `TOTAL_LEN(2 bytes, big-endian) || DATA...`
- If more output remains, host sends another APDU for the same INS with:
  - `P1 = next chunk index`
  - `P2 = param_set` (same parameter set, more bit = 0)
  - `Lc = 0`