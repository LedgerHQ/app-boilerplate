# Protocol specification

The communication protocol used by [BOLOS](https://ledger.readthedocs.io/en/latest/bolos/overview.html) to exchange [APDU](https://en.wikipedia.org/wiki/Smart_card_application_protocol_data_unit) is very close to [ISO 7816-4](https://www.iso.org/standard/77180.html) with a few differences:

- `Lc` length is always exactly 1 byte
- No `Le` field in APDU command
- Maximum size of APDU command is 260 bytes: 5 bytes of header + 255 bytes of data
- Maximum size of APDU response is 260 bytes: 258 bytes of response data + 2 bytes of status word

Status words tend to be similar to common [APDU responses](https://www.eftlab.com/knowledge-base/complete-list-of-apdu-responses/) in the industry.

## Application Protocol Data Unit (APDU)

### Command APDU

| Field name | Length (bytes) | Description |
| --- | --- | --- |
| CLA | 1 | Instruction class - indicates the type of command |
| INS | 1 | Instruction code - indicates the specific command |
| P1 | 1 | Instruction parameter 1 for the command |
| P2 | 1 | Instruction parameter 2 for the command |
| Lc | 1 | The number of bytes of command data to follow (a value from 0 to 255) |
| Data | var | Command data with `Lc` bytes |

### Response APDU

| Field name | Length (bytes) | Description |
| --- | --- | --- |
| Reponse | var | Reponse data (can be empty) |
| SW | 2 | Status word containing command processing status (e.g. `0x9000` for success) |


## Boilerplate commands

### Overview

| Command name | INS | Description |
| --- | --- | --- |
| `GET_VERSION` | 0x03 | Get application version as `MAJOR`, `MINOR`, `PATCH` (3 bytes) |
| `GET_APP_NAME` | 0x04 | Get ASCII encoded application name |

### GET_VERSION

#### Command

| CLA | INS | P1 | P2 | Lc | Payload |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x03 | 0x00 | 0x00 | 0x00 | - |

#### Response

| Response length (bytes) | SW | Description |
| --- | --- | --- |
| 3 | 0x9000 | `MAJOR` (1 byte), `MINOR` (1 byte), `PATCH` (1 byte) |

### GET_APP_NAME

#### Command

| CLA | INS | P1 | P2 | Lc | Payload |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x04 | 0x00 | 0x00 | 0x00 | - |

#### Response

| Response length (bytes) | SW | Description |
| --- | --- | --- |
| var | 0x9000 | ASCII encoded application name of variable length |

### Status Words

| Status Word | SW name | Description |
| --- | --- | --- |
| 0x6A86 | `WRONG_P1P2` | Either `P1` or `P2` is incorrect |
| 0x6A87 | `WRONG_DATA_LENGTH` | `Lc` or minimum APDU lenght is incorrect |
| 0x6D00 | `INS_NOT_SUPPORTED` | No command exists with `INS` |
| 0x6E00 | `CLA_NOT_SUPPORTED` | Bad `CLA` used for this application |
| 0xB000 | `APPNAME_TOO_LONG` | Application name too long (> 64 bytes) |
| 0x9000 | `OK` | Success |
