# Cardano Transaction Processing Overview

This document describes the details of transaction parsing, hashing, and the data structures used within the Ledger application.

## Transaction Body Specification

The transaction body structure follows the Cardano CDDL specification. The authoritative reference for the supported transaction format is [conway.cddl](conway.cddl).

### Supported Fields

The application supports the following fields in the `transaction_body` map:

- `0`: `inputs`
- `1`: `outputs`
- `2`: `fee`
- `3`: `ttl` (optional)
- `4`: `certificates` (optional)
- `5`: `withdrawals` (optional)
- `7`: `auxiliary_data_hash` (optional)
- `8`: `validity_interval_start` (optional)
- `9`: `mint` (optional)
- `11`: `script_data_hash` (optional)
- `13`: `collateral_inputs` (optional)
- `14`: `required_signers` (optional)
- `15`: `network_id` (optional)
- `16`: `collateral_output` (optional)
- `17`: `total_collateral` (optional)
- `18`: `reference_inputs` (optional)
- `19`: `voting_procedures` (optional)
- `21`: `treasury` (optional)
- `22`: `donation` (optional)

*Note: Proposal procedures (`20`) are intentionally NOT supported.*

## Transaction Hashing (`tx_hash_builder.c`)

The `tx_hash_builder` is a stateful component responsible for computing the Blake2b-256 hash of the transaction body. It ensures:

1.  **Canonical CBOR Encoding**: Data is hashed in the exact format required by the Cardano ledger.
2.  **State Machine Validation**: It enforces that fields are added in the correct order and that all expected data (like all tokens in an asset group) has been provided before moving to the next field.
3.  **Efficiency**: It hashes data incrementally to minimize memory usage, which is critical for the restricted environment of a Ledger device.

## Parsing Logic (`tx_parse.c`)

The parser decodes the custom APDU-based serialization format sent by the client into internal C structures.

- **Dynamic Allocation**: It uses a specialized allocator (`app_mem_alloc`) to manage memory during the parsing phase.
- **Linked Lists**: Complex structures like multi-asset outputs or multiple certificates are stored in linked lists (`flist.h`) to handle variable-length data without large static buffers.
- **Validation**: Every field is validated for length, range, and consistency with the transaction metadata provided during the initialization phase.

## UI Planning (`tx_prepare.c`)

Once the transaction is parsed and hashed, `tx_prepare.c` determines how to present the information to the user. It:
- Formats ADA amounts and asset quantities.
- Summarizes complex actions (like certificates).
- Identifies "Change" outputs (device-owned addresses) to avoid unnecessary user confirmation.
