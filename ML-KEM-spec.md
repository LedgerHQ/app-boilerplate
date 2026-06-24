### ML-KEM KEY GENERATION

#### Description

Generates an ML-KEM keypair from deterministic randomness (`coins = d || z`, 64 bytes).

Because ML-KEM objects are larger than a single APDU payload, command data and response data are chunked.

#### Coding

##### `Command`

| CLA | INS | P1          | P2         | Lc       |
| --- | --- | ---         | ---        | ---      |
| E0  | 30  | Chunk index | More/Last  | variable |

##### `Input data`

| Description                     | Length |
| ---                             | ---    |
| `coins = d || z` random seed    | 64     |

##### `Output data`

For example for ML-KEM-512:

| Description     | Length |
| ---             | ---    |
| Public key      | 800    |
| Secret key      | 1632   |

### ML-KEM ENCAPSULATION

#### Description

Encapsulates with ML-KEM using public key and deterministic randomness.

#### Coding

##### `Command`

| CLA | INS | P1          | P2         | Lc       |
| --- | --- | ---         | ---        | ---      |
| E0  | 31  | Chunk index | More/Last  | variable |

##### `Input data`

| Description                              | Length |
| ---                                      | ---    |
| Public key                               | 800    |
| `coins` deterministic encapsulation seed | 32     |

##### `Output data`

For example for ML-KEM-512:

| Description     | Length |
| ---             | ---    |
| Ciphertext      | 768    |
| Shared secret   | 32     |

### ML-KEM DECAPSULATION

#### Description

Decapsulates with ML-KEM using ciphertext and secret key.

#### Coding

##### `Command`

| CLA | INS | P1          | P2         | Lc       |
| --- | --- | ---         | ---        | ---      |
| E0  | 32  | Chunk index | More/Last  | variable |

##### `Input data`

For example for ML-KEM-512:

| Description     | Length |
| ---             | ---    |
| Ciphertext      | 768    |
| Secret key      | 1632   |

##### `Output data`

| Description     | Length |
| ---             | ---    |
| Shared secret   | 32     |

### ML-KEM Chunked response framing

For `INS=0x30/0x31/0x32`, output is streamed in multiple response APDUs as needed:

- Final input APDU response contains the first output chunk
- First output chunk format: `TOTAL_LEN(2 bytes, big-endian) || DATA...`
- If more output remains, host sends another APDU for the same INS with:
  - `P1 = next chunk index`
  - `P2 = 0x00`
  - `Lc = 0`
- Subsequent responses contain only `DATA...`
- Host stops when `TOTAL_LEN` bytes have been collected