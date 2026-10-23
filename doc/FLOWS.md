# APDU Flows

> **Warning**
This documentation is a template and shall be updated with your own flows.

This document is the **dynamic** description of the Boilerplate application: for each feature, which APDUs the
host sends, in which order, and how the application and the user react.

The encoding of each command is not repeated here, it is described in the **static** companion document:
[APDU.md](APDU.md).

- [Conventions](#conventions)
- [Identify the application](#identify-the-application)
- [Get a public key](#get-a-public-key)
- [Verify an address on the device](#verify-an-address-on-the-device)
- [Sign a transaction](#sign-a-transaction)
- [Sign a token transaction](#sign-a-token-transaction)
- [Sign a token transaction with a dynamic token](#sign-a-token-transaction-with-a-dynamic-token)
- [Swap](#swap)
- [Signing context lifecycle](#signing-context-lifecycle)

## Conventions

The diagrams use the following participants:

| Participant | Description |
| --- | --- |
| Host | The software driving the device: Ledger wallet, the Device Management Kit, or the Python client in `tests/` |
| OS | BOLOS, for the commands with `CLA = 0xB0` |
| App | The Boilerplate application |
| User | The person reviewing the screens and approving or rejecting on the device |

The blocks between `<!-- BEGIN GENERATED FLOW -->` and `<!-- END GENERATED FLOW -->` markers must not be edited
by hand: they are generated from the functional tests tagged with `@pytest.mark.flow`, so the diagram and the
screens are the ones of a green test. To regenerate them, from `tests/`:

```shell
pytest standalone --device flex --flows_dir standalone/flows -m flow
python -m standalone.generate_flows --flows_dir standalone/flows
```

## Identify the application

Before any other command, the host checks that the expected application is open and that its version
supports the features it needs.

```mermaid
sequenceDiagram
    participant H as Host
    participant O as OS
    participant A as App

    H->>O: GET_APP_AND_VERSION (B0 01)
    O-->>H: "Boilerplate", version, flags, 9000
    H->>A: GET_VERSION (E0 03)
    A-->>H: major, minor, patch, 9000
    H->>A: GET_APP_NAME (E0 04)
    A-->>H: "Boilerplate", 9000
```

## Get a public key

The host retrieves the public key silently, for example to build the account and its addresses. No screen is
displayed.

```mermaid
sequenceDiagram
    participant H as Host
    participant A as App

    H->>A: GET_PUBLIC_KEY (E0 05, P1=00), BIP32 path
    A->>A: Derive the key on secp256k1
    A-->>H: public key, chain code, 9000
```

## Verify an address on the device

Before receiving funds, the user checks on the device screen that the address shown by the host is really
derived from their seed.

<!-- BEGIN GENERATED FLOW verify_address -->

```mermaid
sequenceDiagram
    participant H as Host
    participant A as App
    actor U as User

    H->>A: GET_PUBLIC_KEY (E0 05) P1=01 P2=00, 21 bytes
    A->>U: Address screens
    alt User approves
        U->>A: Approve
        A-->>H: 99 bytes, 9000
    else User rejects
        U->>A: Reject
        A-->>H: 6985 SWO_CONDITIONS_NOT_SATISFIED
    end
```

<details open>
<summary>User approves: screens on Flex</summary>
<!-- markdownlint-disable MD013 -->
<p>
<a href="../tests/standalone/snapshots/flex/test_get_public_key_confirm_accepted/00000.png"><img src="../tests/standalone/snapshots/flex/test_get_public_key_confirm_accepted/00000.png" alt="Address screen 1" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_get_public_key_confirm_accepted/00001.png"><img src="../tests/standalone/snapshots/flex/test_get_public_key_confirm_accepted/00001.png" alt="Address screen 2" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_get_public_key_confirm_accepted/00002.png"><img src="../tests/standalone/snapshots/flex/test_get_public_key_confirm_accepted/00002.png" alt="Address screen 3" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_get_public_key_confirm_accepted/00003.png"><img src="../tests/standalone/snapshots/flex/test_get_public_key_confirm_accepted/00003.png" alt="Address screen 4" width="120"></a>
</p>
<!-- markdownlint-enable MD013 -->
</details>

<details open>
<summary>User rejects: screens on Flex</summary>
<!-- markdownlint-disable MD013 -->
<p>
<a href="../tests/standalone/snapshots/flex/test_get_public_key_confirm_refused/00000.png"><img src="../tests/standalone/snapshots/flex/test_get_public_key_confirm_refused/00000.png" alt="Address screen 1" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_get_public_key_confirm_refused/00001.png"><img src="../tests/standalone/snapshots/flex/test_get_public_key_confirm_refused/00001.png" alt="Address screen 2" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_get_public_key_confirm_refused/00002.png"><img src="../tests/standalone/snapshots/flex/test_get_public_key_confirm_refused/00002.png" alt="Address screen 3" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_get_public_key_confirm_refused/00003.png"><img src="../tests/standalone/snapshots/flex/test_get_public_key_confirm_refused/00003.png" alt="Address screen 4" width="120"></a>
</p>
<!-- markdownlint-enable MD013 -->
</details>

<!-- END GENERATED FLOW verify_address -->

## Sign a transaction

The host sends the BIP32 path first, then the serialized transaction in chunks of at most 255 bytes. The
review starts when the last chunk is received, and the signature is returned only once the user approves.

<!-- BEGIN GENERATED FLOW sign_tx -->

```mermaid
sequenceDiagram
    participant H as Host
    participant A as App
    actor U as User

    H->>A: SIGN_TX (E0 06) P1=00 P2=80, 21 bytes
    A-->>H: 9000
    alt User approves
        H->>A: SIGN_TX (E0 06) P1=01 P2=00, 49 bytes
        A->>U: Review screens
        U->>A: Approve
        A-->>H: 72 bytes, 9000
    else User rejects
        H->>A: SIGN_TX (E0 06) P1=01 P2=00, 81 bytes
        A->>U: Review screens
        U->>A: Reject
        A-->>H: 6985 SWO_CONDITIONS_NOT_SATISFIED
    end
```

<details open>
<summary>User approves: screens on Flex</summary>
<!-- markdownlint-disable MD013 -->
<p>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_short_tx/00000.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_short_tx/00000.png" alt="Review screen 1" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_short_tx/00001.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_short_tx/00001.png" alt="Review screen 2" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_short_tx/00002.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_short_tx/00002.png" alt="Review screen 3" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_short_tx/00003.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_short_tx/00003.png" alt="Review screen 4" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_short_tx/00004.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_short_tx/00004.png" alt="Review screen 5" width="120"></a>
</p>
<!-- markdownlint-enable MD013 -->
</details>

<details open>
<summary>User rejects: screens on Flex</summary>
<!-- markdownlint-disable MD013 -->
<p>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_refused/00000.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_refused/00000.png" alt="Review screen 1" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_refused/00001.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_refused/00001.png" alt="Review screen 2" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_refused/00002.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_refused/00002.png" alt="Review screen 3" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_refused/00003.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_refused/00003.png" alt="Review screen 4" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_refused/00004.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_refused/00004.png" alt="Review screen 5" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_sign_tx_refused/00005.png"><img src="../tests/standalone/snapshots/flex/test_sign_tx_refused/00005.png" alt="Review screen 6" width="120"></a>
</p>
<!-- markdownlint-enable MD013 -->
</details>

<!-- END GENERATED FLOW sign_tx -->

A transaction whose memo is exactly `Blind-sign` is displayed with the blind signing flow instead of the
regular review: this is a demonstration of the blind signing screens of the SDK, the APDUs are the same.

## Sign a token transaction

Same flow as [Sign a transaction](#sign-a-transaction) with `SIGN_TOKEN_TX`. The token address in the
transaction is looked up in the hardcoded token database (`src/token/token_db.c`) to display the amount with
the right ticker and decimals.

```mermaid
sequenceDiagram
    participant H as Host
    participant A as App
    actor U as User

    H->>A: SIGN_TOKEN_TX (E0 07, P1=00, P2=80), BIP32 path
    A-->>H: 9000
    H->>A: SIGN_TOKEN_TX (E0 07, P1=01, P2=00), token transaction
    A->>A: Parse the transaction, look up the token address
    alt Token unknown
        A-->>H: 6A80
    else Token found
        A->>U: Display amount with ticker, recipient, memo
        U->>A: Approve
        A-->>H: signature length, DER signature, v, 9000
    end
```

## Sign a token transaction with a dynamic token

When the token is not in the hardcoded database, or to override its metadata, the host provides a descriptor
signed by the Ledger CAL before the transaction. The OS must first know the CAL key: the host loads its
certificate, then sends the descriptor, then the transaction.

<!-- BEGIN GENERATED FLOW dynamic_token -->

```mermaid
sequenceDiagram
    participant H as Host
    participant O as OS
    participant A as App
    actor U as User

    H->>O: LOAD_CERTIFICATE (B0 06) P1=08 P2=00, 167 bytes
    O-->>H: 9000
    H->>A: PROVIDE_TOKEN_INFO (E0 22) P1=00 P2=00, 140 bytes
    A-->>H: 9000
    H->>A: SIGN_TOKEN_TX (E0 07) P1=00 P2=80, 21 bytes
    A-->>H: 9000
    H->>A: SIGN_TOKEN_TX (E0 07) P1=01 P2=00, 86 bytes
    A->>U: Review screens
    U->>A: Approve
    A-->>H: 72 bytes, 9000
```

<details open>
<summary>Nominal: screens on Flex</summary>
<!-- markdownlint-disable MD013 -->
<p>
<a href="../tests/standalone/snapshots/flex/test_provide_dynamic_token_valid_new/00000.png"><img src="../tests/standalone/snapshots/flex/test_provide_dynamic_token_valid_new/00000.png" alt="Review screen 1" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_provide_dynamic_token_valid_new/00001.png"><img src="../tests/standalone/snapshots/flex/test_provide_dynamic_token_valid_new/00001.png" alt="Review screen 2" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_provide_dynamic_token_valid_new/00002.png"><img src="../tests/standalone/snapshots/flex/test_provide_dynamic_token_valid_new/00002.png" alt="Review screen 3" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_provide_dynamic_token_valid_new/00003.png"><img src="../tests/standalone/snapshots/flex/test_provide_dynamic_token_valid_new/00003.png" alt="Review screen 4" width="120"></a>
<a href="../tests/standalone/snapshots/flex/test_provide_dynamic_token_valid_new/00004.png"><img src="../tests/standalone/snapshots/flex/test_provide_dynamic_token_valid_new/00004.png" alt="Review screen 5" width="120"></a>
</p>
<!-- markdownlint-enable MD013 -->
</details>

<!-- END GENERATED FLOW dynamic_token -->

The dynamic token stays in RAM until the application exits: the host does not need to resend it for each
transaction with the same token. Only one token is kept at a time.

## Swap

In a swap, the user reviews and approves the whole operation in the Exchange application. Exchange then starts
the Boilerplate application as a library to sign the transaction that sends the funds, without displaying it
again: the application only checks that the transaction matches what the user approved.

The commands sent to Exchange are only summarized here, they are described in the
[Exchange documentation](https://ledgerhq.github.io/app-exchange/).

Boilerplate is the FROM application here: the coin that is sent. The payout address and amount are checked
in the same way by the TO application.

```mermaid
sequenceDiagram
    participant H as Host
    participant E as Exchange app
    participant A as Boilerplate app (library)
    actor U as User

    H->>E: START_NEW_TRANSACTION, partner and proposal commands
    E-->>H: 9000
    H->>E: CHECK_PAYOUT_ADDRESS
    Note over E: Payout address and amount checked by the TO application
    E-->>H: 9000
    H->>E: CHECK_REFUND_ADDRESS
    E->>A: os_lib_call CHECK_ADDRESS (BIP32 path, refund address)
    A-->>E: Address derived from the device seed or not
    E->>A: os_lib_call GET_PRINTABLE_AMOUNT (amount)
    A-->>E: Formatted amount with ticker
    E->>A: os_lib_call GET_PRINTABLE_AMOUNT (fees)
    A-->>E: Formatted fees in BOL
    E-->>H: 9000
    H->>E: PROMPT_UI_DISPLAY
    E->>U: Display the swap
    U->>E: Approve
    E-->>H: 9000
    H->>E: START_SIGNING_TRANSACTION
    E-->>H: 9000
    E->>A: os_lib_call SIGN_TRANSACTION (amount, fees, recipient, coin configuration)
    A->>A: Store the approved values
    opt Dynamic token
        H->>A: PROVIDE_TOKEN_INFO (E0 22), signed descriptor
        A-->>H: 9000
    end
    H->>A: SIGN_TX or SIGN_TOKEN_TX (P1=00, P2=80), BIP32 path
    A-->>H: 9000
    H->>A: SIGN_TX or SIGN_TOKEN_TX (P1=01, P2=00), transaction
    A->>A: Parse, check amount, fees, recipient and token against the approved values
    alt All values match
        A->>A: Sign without display
        A-->>H: signature length, DER signature, v, 9000
    else Mismatch or parsing error
        A-->>H: error code, application code, C000
    end
    A->>E: Exit the library
    E->>U: Display the signing status
```

The application signs at most one transaction per swap, and exits after the first response.

On failure, the response data holds 2 bytes before the `C000` status word:

| Common error code | Application code | Reason |
| --- | --- | --- |
| `01` | `00` | Amount differs from the approved one |
| `01` | `01` | Token ticker or decimals differ from the approved ones |
| `02` | `00` | Recipient differs from the approved one |
| `03` | `00` | Fees differ from the approved ones |
| `04` | `00` | Native transaction received for a token swap, or the reverse |
| `FF` | `00` | Transaction cannot be parsed, unknown token, or swap not initialized |

## Signing context lifecycle

`SIGN_TX` and `SIGN_TOKEN_TX` share one signing context. Whatever the current state, a first chunk
(`P1 = 0x00`) resets it.

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> PathReceived: P1=00 P2=80, BIP32 path
    PathReceived --> Collecting: P1=01..03 P2=80
    Collecting --> Collecting: P1=01..03 P2=80
    PathReceived --> Review: P1=01..03 P2=00, parsing OK
    Collecting --> Review: P1=01..03 P2=00, parsing OK
    Review --> Idle: Approve, signature returned
    Review --> Idle: Reject, 6985
    PathReceived --> PathReceived: P1=00 P2=80, context reset
    Collecting --> PathReceived: P1=00 P2=80, context reset
```

After an error status word, the host must restart the transaction from the first chunk. `GET_PUBLIC_KEY`
also resets the signing context: it must not be sent between two chunks of a transaction.
