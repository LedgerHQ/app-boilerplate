# Ledger Boilerplate Application

This is a boilerplate application which can be forked to start a new project for the Ledger Nano S/X.

## Prerequisite

### With the docker image builder

The app-builder docker image [from this repository](https://github.com/LedgerHQ/ledger-app-builder) contains all needed tools and library to build and load an application.
You can download it from the ghcr.io docker repository:

```shell
sudo docker pull ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-full
```

You can then enter this development environment by executing the following command from the directory of the application `git` repository:

```shell
sudo docker run --rm -ti --user "$(id -u)":"$(id -g)" -v "$(realpath .):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-full
```

The application's code will be available from inside the docker container, you can proceed to the following compilation steps to build your app.

### Without the docker image builder

Be sure to have your environment correctly set up (see [Getting Started](https://developers.ledger.com/docs/nano-app/introduction/)) and [ledgerblue](https://pypi.org/project/ledgerblue/) and installed.

If you want to benefit from [vscode](https://code.visualstudio.com/) integration, it's recommended to move the toolchain in `/opt` and set `BOLOS_ENV` environment variable as follows

```shell
BOLOS_ENV=/opt/bolos-devenv
```

and do the same with `BOLOS_SDK` environment variable

```shell
BOLOS_SDK=/opt/nanos-secure-sdk
```

## Compilation and load

```shell
make DEBUG=1  # compile optionally with PRINTF
make load     # load the app on the Nano using ledgerblue
```

## Documentation

High level documentation such as [APDU](doc/APDU.md), [commands](doc/COMMANDS.md) and [transaction serialization](doc/TRANSACTION.md) are included in developer documentation which can be generated with [doxygen](https://www.doxygen.nl)

```shell
doxygen .doxygen/Doxyfile
```

the process outputs HTML and LaTeX documentations in `doc/html` and `doc/latex` folders.

## Tests & Continuous Integration

The flow processed in [GitHub Actions](https://github.com/features/actions) is the following:

- Code formatting with [clang-format](http://clang.llvm.org/docs/ClangFormat.html)
- Compilation of the application for Ledger Nano S in [ledger-app-builder](https://github.com/LedgerHQ/ledger-app-builder)
- Unit tests of C functions with [cmocka](https://cmocka.org/) (see [unit-tests/](unit-tests/))
- End-to-end tests with [Speculos](https://github.com/LedgerHQ/speculos) emulator (see [tests/](tests/))
- Code coverage with [gcov](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html)/[lcov](http://ltp.sourceforge.net/coverage/lcov.php) and upload to [codecov.io](https://about.codecov.io)
- Documentation generation with [doxygen](https://www.doxygen.nl)

It outputs 4 artifacts:

- `boilerplate-app-debug` within output files of the compilation process in debug mode
- `speculos-log` within APDU command/response when executing end-to-end tests
- `code-coverage` within HTML details of code coverage
- `documentation` within HTML auto-generated documentation

## Are you developing a Nano S, S Plus, X application?

If so, This boilerplate will help you get started.

For a smooth and quick integration:

- See the developers’ documentation on the [Developer Portal](https://developers.ledger.com/), and
- [Go on Discord](https://developers.ledger.com/discord-pro/) to chat with developer support and the developer community.
