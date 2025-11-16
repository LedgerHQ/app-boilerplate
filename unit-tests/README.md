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
