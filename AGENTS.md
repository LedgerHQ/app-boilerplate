We are converting old version of Ledger Cardano app into a new modernized version, with changed UI.
Old app / shelley app: ../app-cardano
You should look into the code of the old app (instead of guessing how to process something).
Most of code has already been copied to the new app, so before copying anything you have to search new app repository first.

New app: ../ledger-app-cardano
The "new app" is a fork of boilerplate app. If you find any traces of boilerplate or apparently useless leftover code in the new app, ask about removing them.

We are no longer supporting Nano S, and all features will be included for every device.
Only nbgl library will be used for UI, but in a different fashion than in the old app (different UI flow, but nbgl screens should be good).

App architecture:
src contains C code, cannot be compiled directly (ledger plugin for vs code with appropriate docker containers is used by hand).
When app is build for a specific device (we use Stax), it can receive APDU from Python client.
When APDU is received, it is handled by dispatcher.c and then by the right handler. Each handler parses input, checks it is valid and not dangerous (see securityPolicy.c --- this is especially important for every bip44 path received), computes return value, and prepares UI to display. We want the code to be consistent across different handlers as much as reasonable.

Python client that sends APDU:
../ledger-app-cardano/tests/application_client
Ragger tests:
../ledger-app-cardano/tests/standalone
(eventually will cover all codebase, at this point they are somewhat broken and copied from the old app)
Test fixtures:
../ledger-app-cardano/tests/standalone/input_files
Tests are run by hand.

Transaction body uses CBOR (via CDDL spec), do not modify code in txhashbuilder.c, it is trusted and correct, so is addressUtilsShelley.c and bip44.c.
Do not add any CBOR serialization or address manipulation or bip44 path manipulations functions on your own (if it seems necessary, ask first).
Order of items in transaction body (should be followed in general when organizing code and ordering UI display items):
In raw_tx (or elsewhere), we do not serialize constants (e.g. if some item is always 28 bytes, both C and Python should have a named constant for that length and apply it, no need to serialize 28 as a prefix in any buffer).

transaction_body = 
  {   0  : set<transaction_input>  // inputs
  ,   1  : [* transaction_output]  // outputs
  ,   2  : coin  // fee
  , ? 3  : slot_no  // ttl
  , ? 4  : certificates
  , ? 5  : withdrawals
  , ? 7  : auxiliary_data_hash
  , ? 8  : slot_no  // validity interval
  , ? 9  : mint
  , ? 11 : script_data_hash
  , ? 13 : nonempty_set<transaction_input>  // collateral inputs
  , ? 14 : required_signers
  , ? 15 : network_id 
  , ? 16 : transaction_output  // collateral output
  , ? 17 : coin  // total collateral
  , ? 18 : nonempty_set<transaction_input>  // reference inputs
  , ? 19 : voting_procedures
  , ? 20 : proposal_procedures
  , ? 21 : coin  // treasury
  , ? 22 : positive_coin  // donation
  }

Style:
Use longer more clear variable names.
Correctness and security are paramount. Use lots of STATIC_ASSERT and LEDGER_ASSERT wherever appropriate, check in this way for function parameters, invariants in loops and state machine checks, etc. Make sure no memory bugs, overflows etc. appear. Use TRACE liberally to help debugging (prefer against PRINTF). Memory is scarce, so data stored in global context during whole transaction processing (e.g. in raw tx buffer) should really be needed repeatedly, and not just temporarily created/destroyed at a single point. Never use forward declarations, instead suggest how to better organize imports (must be confirmed before coding).

Additional resources:
BOLOS SDK (underlying library for system calls, crypto, nbgl lib):
../../ledger/ledger-secure-sdk
Ethereum eth app (modern code you will be ask to consult occasionally to copy or mimic):
../../ledger/app-ethereum
Bitcoin btc app (modern code you will be ask to consult occasionally to copy or mimic):
../../ledger/app-bitcoin-new
Ledgerjs, typescript API to be used with companion apps:
../ledgerjs-cardano-shelley

You can read git commits or changes from the last commit, but do not do any git operations/modifications/writes.

