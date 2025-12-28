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

## Deriving Test Keys

To add mock cryptographic keys to `mocks/crypto_mock_data.h`, use the ragger library's `calculate_public_key_and_chaincode` function with the standard test mnemonic:

```python
from ragger.bip import calculate_public_key_and_chaincode, CurveChoice
import hashlib

# Standard test mnemonic used in all unit tests
mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"

# Example: Derive pool cold key m/1853'/1815'/0'/0'
path_str = "m/1853'/1815'/0'/0'"
ref_pk, ref_chain_code = calculate_public_key_and_chaincode(
    CurveChoice.Ed25519Kholaw,
    path_str,
    mnemonic=mnemonic
)

# Remove the "00" prefix from the public key
pub_key = bytes.fromhex(ref_pk[2:])
chain_code_bytes = bytes.fromhex(ref_chain_code)

# Optionally verify the Blake2b-224 hash
key_hash = hashlib.blake2b(pub_key, digest_size=28).digest()
print(f"Key hash: {key_hash.hex()}")
```

**Important**: Always pass the `mnemonic` parameter to `calculate_public_key_and_chaincode`. Without it, the function uses ragger's default seed which will produce different keys than the test fixtures expect.