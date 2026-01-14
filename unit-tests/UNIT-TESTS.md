# Unit tests

## Prerequisite

Be sure to have installed:

- CMake >= 3.10
- CMocka >= 1.1.5

and for code coverage generation:

- lcov >= 1.14

On Ubuntu, the following command will install the required dependencies:

```shell
sudo apt install cmake libcmocka-dev lcov
```

## Overview

In `unit-tests` folder, compile with

```shell
cmake -Bbuild -H. && make -C build
```

and run tests with

```shell
CTEST_OUTPUT_ON_FAILURE=1 make -C build test
```

To run a specific test binary (e.g., `test_ui_formatters`), use:

```shell
cd unit-tests
cmake -Bbuild -H. && make -C build
CTEST_OUTPUT_ON_FAILURE=1 ctest --test-dir build -R test_ui_formatters
```

## Generate code coverage

Just execute in `unit-tests` folder

```shell
./gen_coverage.sh
```

it will output `coverage.total` and `coverage/` folder with HTML details (in `coverage/index.html`).

## Structure

- Test files are placed directly in `unit-tests/` directory with `test_*.c` naming pattern
- `mock_includes/` contains SDK header mocks for native compilation
- `libs/` contains mock implementations (crypto, etc.)
- Each test file tests a specific module from `../src/`

## Test Fixture Generation

Some unit tests consume generated C headers. Do not hand-edit these generated files; update the generator scripts and re-run them.

### Transaction Signing Fixtures

Fixtures for sign-tx tests are generated from LedgerJS fixtures and serialized through the shared Python command builder.

Generators (run from repo root with the standalone venv):

```bash
source tests/standalone/venv/bin/activate
python3 unit-tests/generate_unit_tests_from_ragger.py
# or individual steps:
python3 unit-tests/generate_unit_tests_from_ragger.py fixtures
python3 unit-tests/generate_unit_tests_from_ragger.py generate-test-runners
python3 unit-tests/generate_unit_tests_from_ragger.py rejects
```

The default command runs all generators in order (fixtures, complete-tests, rejects, mock-data).

Notes:
- `unit-tests/generate_unit_tests_from_ragger.py` produces `unit-tests/test_sign_tx_fixtures_*.h` from
  `tests/standalone/input_files/signTx.py`, rewrites each `unit-tests/test_sign_tx_*.c`
  with `tx_fixture_t` + `run_fixture_with_expert_mode`, and emits `unit-tests/test_sign_tx_fixtures_rejects.h`.
- `unit-tests/export_sign_tx_rejects.js` runs inside `../ledgerjs-cardano-shelley` (via Node)
  to export reject fixtures that are then serialized with `tests/application_client/command_builder.py`.
- APDU fixtures use the app’s binary schema (presence flags + length-prefixed ASCII for relays/metadata);
  they are not CBOR byte dumps from LedgerJS. CBOR fixtures remain the source of truth for tx body/hash validation.

Suggested improvements:
- Add a single `unit-tests/generate_fixtures.py` entry point that runs all generators in order
  (all fixtures, complete tests, rejects, mock data) and verifies a clean diff.
- Extract shared helper code (path setup, base58 shim, command builder utilities) into a small
  `unit-tests/fixture_utils.py` to reduce duplication across generators.

### Mock Crypto Fixtures

Mock key material lives in `unit-tests/mocks/crypto_mock_data.h` and is regenerated with:

```bash
source tests/standalone/venv/bin/activate
python3 unit-tests/generate_unit_tests_from_ragger.py mock-data
```

This updates public keys, chain codes, and key hashes while preserving signature vectors.
The file is rewritten in place.

## Regenerating Mock Data

To regenerate all mock cryptographic data in `mocks/crypto_mock_data.h`:

```bash
cd unit-tests
# Activate the ragger venv (required!)
source ../tests/standalone/venv/bin/activate
# Run the regeneration script
python3 generate_unit_tests_from_ragger.py mock-data
```

The script:
- Uses the standard test mnemonic: "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"
- Derives public keys and chain codes using ragger's `calculate_public_key_and_chaincode` with `CurveChoice.Ed25519Kholaw`
- Computes Blake2b-224 key hashes for Cardano addresses
- Preserves signatures as-is (they're test vectors for internal C functions)

**Important**: The script requires ragger to be installed. Always run it from within the tests/standalone/venv.

# Mock Data Verification Tests

This document describes the verification system for mock cryptographic data used in unit tests.

## Overview

The mock data in `mocks/crypto_mock_data.h` contains hardcoded key material (public keys, chain codes) and test signatures. These tests ensure that:

1. **All key material is correctly derived** from the standard test mnemonic
2. **Key hashes are correctly computed** as Blake2b-224 hashes of public keys
3. **Opcert messages** are properly constructed for signing

## Standard Test Mnemonic

All mock data is derived from this standard mnemonic:

```
"abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"
```

This is the standard test mnemonic used across all Cardano Ledger application tests for consistency.

## Test Coverage

### 1. Key Derivation Verification (`unit-tests/test_mock_key_derivation.c`)

**What it does:**
- Parses `crypto_mock_data.h` to extract all 21 mock path entries
- For each entry, derives keys from the standard mnemonic using ragger
- Verifies that public key, chain code, and key hash match

**How to run:**
```bash
cd unit-tests
cmake -Bbuild -H. && make -C build test
# or specifically:
CTEST_OUTPUT_ON_FAILURE=1 make -C build test 2>&1 | grep test_mock_key_derivation
```

**Expected result:** ✓ All 21 entries verified successfully

### 2. Ragger Test for Mock Key Derivation (`tests/standalone/test_mock_key_derivation.py`)

**What it does:**
- Python version of key derivation verification
- Dynamically parses the C header file
- Derives keys from the standard mnemonic using ragger's `calculate_public_key_and_chaincode`
- Verifies public key, chain code, and Blake2b-224 key hash

**How to run:**
```bash
cd tests/standalone
pytest -xvs --device stax test_mock_key_derivation.py::test_all_mock_key_derivation
```

**Key paths verified:**
- Governance paths (DRep, Committee hot keys)
- Pool cold keys
- Account keys
- All standard Cardano BIP-44 paths

### 3. Opcert Message Construction (`unit-tests/test_opcert_message.c`)

**What it does:**
- Verifies that opcert messages are constructed correctly
- Tests message structure: KES public key (32 bytes) || Issue counter (8 bytes) || KES period (8 bytes)
- Tests with actual values from ragger opcert tests

**Test vectors from ragger tests:**
- Path: `m/1853'/1815'/0'/0'` (pool cold key)
- KES public key: `3d24bc547388cf2403fd978fc3d3a93d1f39acf68a9c00e40512084dc05f2822`
- KES period: 47
- Issue counter: 42

### 4. Ragger Opcert Test (`tests/standalone/test_opcert.py`)

**What it does:**
- Actually signs operational certificates using the Ledger app
- Verifies the signature against the constructed message
- Tests with actual Ed25519 signature verification

**How to run:**
```bash
cd tests/standalone
pytest -xvs --device stax test_opcert.py::test_opCert
```

## Mock Signature Data

The `MOCK_SIGNATURES` array in `mocks/crypto_mock_data.h` contains pre-computed Ed25519 signatures for testing. These signatures are used by unit tests that call the mock `crypto_eddsa_sign` function.

**Note**: These signatures are derived from the standard test mnemonic and the message buffers stored in `mocks/crypto_mock_data.h`. If you update a message buffer (for example, CVote payload hashes), rerun the regeneration script to keep signatures consistent.

## Regenerating Mock Data

If mock data (public keys, chain codes, key hashes) becomes outdated or incorrect:

```bash
cd unit-tests
# Activate the ragger venv (required!)
source ../tests/standalone/venv/bin/activate
python3 generate_unit_tests_from_ragger.py mock-data
# Review the changes in crypto_mock_data_regenerated.h
mv mocks/crypto_mock_data_regenerated.h mocks/crypto_mock_data.h
```

This script:
- Derives all public keys and chain codes from the standard test mnemonic
- Calculates Blake2b-224 key hashes
- Regenerates Ed25519 signatures using the standard mnemonic and the message buffers
- Updates `mocks/crypto_mock_data.h` with correct key material

## Key Governance Paths

The implementation uses these standardCIP-1852 paths for governance:

- **DRep Key**: `m/1852'/1815'/0'/3/0` - For governance delegation
- **Committee Hot Key**: `m/1852'/1815'/0'/5/0` - For committee authorization
- **Pool Cold Key**: `m/1853'/1815'/0'/0'` - For operational certificates
- **Standard Address Keys**: `m/1852'/1815'/0'/0/x` - Standard account keys

All these paths are verified in the mock data tests.

## Adding New Mock Entries

To add a new mock path entry:

1. Add the path to `mocks/crypto_mock_data.h` MOCK_PATHS array
2. Run `python3 generate_unit_tests_from_ragger.py mock-data` to compute key material
3. The key derivation test will automatically verify the new entry
4. If the test fails, check that the path is correct

## Test Maintenance

These tests should be run:
- **Before commits** to ensure mock data is valid
- **After updating the test mnemonic** to regenerate all data
- **After adding new paths** to governance implementations
- **As part of CI/CD pipeline** to catch regressions

## References

- [CIP-0003: Ledger Nano S HD Wallet Derivation](https://cips.cardano.org/cips/cip3/)
- [CIP-1852: HD Wallets for Plutus](https://cips.cardano.org/cips/cip1852/)
- Ragger Library: `calculate_public_key_and_chaincode` with Ed25519Kholaw curve
- Cardano Address Standards: Blake2b-224 key hashing
