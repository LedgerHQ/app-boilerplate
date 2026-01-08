# Catalyst (CVote) Registration

This document captures the current plan for Catalyst registration support in
the modernized Ledger Cardano app. It replaces the older jm/catalyst notes and
aligns with the new SIGN_TX protocol and NBGL UI approach.

## Scope
- **In scope:** CIP-15/CIP-36 Catalyst registration metadata inside a
  transaction (`auxiliary_data_hash`).
- **Out of scope:** CIP-62 vote-cast messages and any non-registration flows.

## Current Gaps (jm/catalyst2)
- **Hash builder:** already implemented (`src/cvote/aux_data_hash_builder.*`).
- **Security policies:** already defined (`src/securityPolicy/securityPolicy.c`).
- **APDU handling:** now required for the new AUX_DATA stage and CVote handler.
- **UI flow:** not implemented yet.

## Design Summary
The device computes the auxiliary data hash incrementally while receiving CVote
registration fields. We never trust a host-provided hash and do not store raw
registration bytes. Instead, we store only UI strings and minimal state needed
to finish the hash and signature.

### Key decisions
- **Aux data is processed before allocating the transaction raw buffer.**
  This gives more RAM headroom for UI strings.
- **Use a single `nbgl_useCaseReviewLight` for now** to show all registration
  data at once. No streaming mode is implemented in the current phase.
- **First CVote APDU carries all non-registration data**, and **each subsequent
  APDU carries exactly one registration entry** (delegation).

## APDU & State Machine (high level)
1. **SIGN_TX / TX_INIT**: same as current flow, except auxiliary data is
   declared as `AUX_DATA_TYPE_CVOTE_REGISTRATION` without providing a hash.
2. **SIGN_TX / TX_AUX_DATA**: new P1 value; carries CVote registration data.
3. **UI approval**: will happen after all CVote data is received.
4. **SIGN_TX / TX_DATA_CHUNK**: normal transaction body chunks, now with the
   computed `auxDataHash` already in context.

The handler must enforce ordering and reject interleaving, consistent with
existing SIGN_TX protection in `dispatcher.c`.

## CVote AUX_DATA Protocol (planned)
This is the **planned** encoding for the simplified APDU flow. The first
APDU carries all non-registration fields; each subsequent APDU carries one
registration (delegation).

### SIGN_TX P1 Values (planned)
- `P1_TX_INIT = 0x00` (existing)
- `P1_TX_DATA_CHUNK = 0x01` (existing)
- `P1_TX_CHUNK_LAST = 0x02` (existing)
- `P1_TX_AUX_DATA = 0x03` (new)

### TX_INIT Changes (planned)
The TX_INIT payload stays the same, except:
- If `includeAuxDataHash = true`, the host must also send `auxDataType`.
- If `auxDataType == AUX_DATA_TYPE_CVOTE_REGISTRATION`, the hash bytes are **not**
  included in TX_INIT (the device computes them from AUX_DATA APDUs).
- If `auxDataType == AUX_DATA_TYPE_ARBITRARY_HASH`, the hash bytes are sent as
  today.

### AUX_DATA Sub-Instructions (P2 values)
- `AUX_DATA_INIT = 0x36`
- `AUX_DATA_REGISTRATION = 0x37`

#### 1) AUX_DATA_INIT (P1=0x03, P2=0x36)
Single APDU containing the fixed registration header (**all non-delegation data**).

**Fields (in order):**
1. `format` (u8): `CIP15 = 1`, `CIP36 = 2`
2. `delegation_count` (u16, BE)  
   - `CIP15`: must be `0`
   - `CIP36`: `0..65535`
3. `staking_key` (path or raw pubkey)
4. `reward_address` (raw bytes, Shelley format)
5. `nonce` (u64, BE)
6. `voting_purpose` (u64, BE)  
   - present **only** for `CIP36` (send `0` when not provided)
7. `vote_key` (path or raw pubkey)  
   - present for `CIP15`
   - for `CIP36`, present **only** when `delegation_count == 0`

#### 2) AUX_DATA_REGISTRATION (P1=0x03, P2=0x37)
Repeated APDU, one per registration (delegation).

**Fields (in order):**
1. `vote_key` (path or raw pubkey)
2. `weight` (u32, BE)

The number of registration APDUs must match `delegation_count`.

### Expected Order
1. `P1_TX_INIT`
2. `P1_TX_AUX_DATA` (INIT)
3. `P1_TX_AUX_DATA` (REGISTRATION) × N
4. UI approval (single review)
5. `P1_TX_DATA_CHUNK` / `P1_TX_CHUNK_LAST`

### Rejection Conditions (non-exhaustive)
- Unknown `format` or invalid delegation count.
- Registration APDUs not matching the declared count.
- Any BIP44 path rejected by `securityPolicy.c`.
- Instruction interleaving (enforced by dispatcher).
- Payload length mismatch or invalid field encoding.

## UI Strategy (NBGL)
- **Single review only:** all registration items are shown in a single
  `nbgl_useCaseReviewLight` call.
- UI items are assembled as strings during APDU parsing and stored in memory.
- The displayed order must match the metadata order:
  vote key(s) / delegations → staking key → reward address → nonce →
  voting purpose (CIP-36 only).

## Memory Plan
- **Target budget:** ~20 KB for UI strings and pair metadata.
- **Goal:** support ~50 delegations in memory for single review.

Only UI strings are stored. Raw APDU payloads are never buffered. The rolling
hash is updated as data arrives.

## Hashing & Signature
- Use `auxDataHashBuilder_*` incrementally.
- For CIP-15: add the single vote key.
- For CIP-36: enter delegations and add each `(vote key, weight)` pair.
- Add staking key, reward address, nonce, and voting purpose (if present).
- Compute CVote registration signature via `getCVoteRegistrationSignature`.
- Finalize and store `auxDataHash` in the transaction context.

## Security & Validation
- Validate **every BIP44 path** via `securityPolicy.c` before use.
- Enforce state machine invariants with `LEDGER_ASSERT`.
- Verify format, delegation counts, and bounds (e.g., max delegations).
- Display all registration fields to the user unless policy explicitly allows
  hiding (default is `POLICY_SHOW`).

## Implementation Notes (current phase)
- C side: implement only the AUX_DATA **state machine and ordering guards**.
  No hashing, UI, or parsing beyond the minimal count tracking.
- Python side: implement **serialization for the simplified AUX_DATA APDUs**.
- Raw transaction buffer allocation must be **delayed until AUX_DATA completes**.

## References
- `doc/OVERVIEW.md` for app structure.
- `doc/TX.md` for hash integration.
- `src/cvote/aux_data_hash_builder.*` for incremental hashing API.
- Old app (`../app-cardano`) for legacy CVote logic.
