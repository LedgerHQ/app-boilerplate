# Technical Specification

> **Warning**
This documentation is a template and shall be updated with your own APDUs.

## About

This documentation describes the APDU messages interface to communicate with the Boilerplate application.

The application covers the following functionalities :

  - Get a public Boilerplate address given a BIP 32 path
  - Sign a basic Boilerplate transaction given a BIP 32 path and raw transaction
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
| Number of BIP 32 derivations to perform (max 10)                 | 1      |
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

| CLA | INS  | P1                   | P2                               | Lc       | Le       |
| --- | ---  | ---                  | ---                              | ---      | ---      |
| E0  | 06   |  00-FF : chunk index | 00 : last transaction data block | variable | variable |
|     |      |                      | 80 : subsequent transaction data block |    |          |

##### `Input data (first transaction data block)`

| Description                                          | Length   | 
| ---                                                  | ---      | 
| Number of BIP 32 derivations to perform (max 10)     | 1        |
| First derivation index (big endian)                  | 4        |
| ...                                                  | 4        |
| Last derivation index (big endian)                   | 4        |
  
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


## Status Words

The following standard Status Words are returned for all APDUs.

##### `Status Words`


| SW       | SW name                     | Description                                           |
| ---      | ---                         | ---                                                   |
|   6985   | SW_DENY	                   | Rejected by user                                      |
|   6A86   | SW_WRONG_P1P2               | Either P1 or P2 is incorrect                          |
|   6A87   | SW_WRONG_DATA_LENGTH        | Lc or minimum APDU length is incorrect                |
|   6D00   | SW_INS_NOT_SUPPORTED	       | No command exists with INS                            |
|   6E00   | SW_CLA_NOT_SUPPORTED        | Bad CLA used for this application                     |
|   B000   | SW_WRONG_RESPONSE_LENGTH    | Wrong response length (buffer size problem)           |
|   B001   | SW_DISPLAY_BIP32_PATH_FAIL  | BIP32 path conversion to string failed                |
|   B002   | SW_DISPLAY_ADDRESS_FAIL     | Address conversion to string failed                   |
|   B003   | SW_DISPLAY_AMOUNT_FAIL      | Amount conversion to string failed                    |
|   B004   | SW_WRONG_TX_LENGTH	         | Wrong raw transaction length                          |
|   B005   | SW_TX_PARSING_FAIL          | Failed to parse raw transaction                       |
|   B006   | SW_TX_HASH_FAIL	           | Failed to compute hash digest of raw transaction      |
|   B007   | SW_BAD_STATE                | Security issue with bad state                         |
|   B008   | SW_SIGNATURE_FAIL           | Signature of raw transaction failed                   |
|   9000   | OK	                         | Success                                               |