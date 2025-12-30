# Cardano Ledger Application

This is the Ledger application for the Cardano blockchain, providing secure storage and transaction signing for ADA and Cardano native tokens.

## Overview

The Cardano Ledger app allows users to manage their Cardano assets on Ledger hardware devices (Stax, Flex, Nano X, Nano S+). It supports a wide range of transaction types across different Cardano eras, including Shelley, Allegra, Mary, Alonzo, Babbage, and Conway.

For detailed information about the application, please refer to:
- [doc/OVERVIEW.md](doc/OVERVIEW.md): High-level architecture, data flow, and testing infrastructure.
- [doc/TX.md](doc/TX.md): Detailed transaction body processing and hashing specifications.
- [AGENTS.md](AGENTS.md): Development guidelines and instructions for AI agents.

## Quick start guide

TODO: Update quick start guide with Cardano-specific environment details.

### With VSCode

You can setup a convenient environment to build and test your application using [Ledger's VSCode developer tools extension](https://marketplace.visualstudio.com/items?itemName=LedgerHQ.ledger-dev-tools).

1. Install and run [Docker](https://www.docker.com/products/docker-desktop/).
2. Install [VSCode](https://code.visualstudio.com/download) and add [Ledger's extension](https://marketplace.visualstudio.com/items?itemName=LedgerHQ.ledger-dev-tools).
3. Open this folder in VSCode.
4. Use the extension's sidebar to build, run with Speculos, or execute functional tests.

### With a terminal

TODO: Verify and update terminal-based workflow for the current repository structure.

## Compilation and load

TODO: Update compilation instructions to reflect supported devices and Cardano-specific build flags.

*Note: Nano S is no longer supported.*

### Compilation

```shell
make DEBUG=1  # compile optionally with TRACE enabled
```

Supported `BOLOS_SDK` targets:
- `STAX_SDK`
- `FLEX_SDK`
- `NANOX_SDK`
- `NANOSP_SDK`

## Test

The application includes both unit tests and functional tests.

### Functional Tests (Ragger)

Functional tests are located in `tests/standalone/` and use the [Ragger](https://github.com/LedgerHQ/ragger) framework.

```shell
pip install -r tests/standalone/requirements.txt
pytest tests/standalone/ --tb=short -v --device stax
```

### Unit Tests (CMocka)

Unit tests are located in `unit-tests/` and use the [CMocka](https://cmocka.org/) framework. Detailed instructions are in [unit-tests/UNIT-TESTS.md](unit-tests/UNIT-TESTS.md).

## Documentation

- Architecture Overview: [doc/OVERVIEW.md](doc/OVERVIEW.md)
- Transaction Processing: [doc/TX.md](doc/TX.md)
- Unit Tests: [unit-tests/UNIT-TESTS.md](unit-tests/UNIT-TESTS.md)
- Fuzzing: [fuzzing/FUZZING.md](fuzzing/FUZZING.md)

TODO: Update Doxygen configuration to include Cardano-specific documentation modules.

## Continuous Integration

The project uses GitHub Actions for CI, including guidelines enforcement, code formatting checks, compilation for all supported devices, unit tests, and functional tests.