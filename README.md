# Ledger Boilerplate Application

This is a boilerplate application which can be forked to start a new project for the Ledger Nano S/X/SP and Stax.

## Quick start guide

### With VSCode and the docker development tools image

You can quickly setup a convenient environment to build and test your application by using the vscode integration and the [ledger-app-dev-tools]() docker image.

It will allow you, whether you are developing on macOS, Windows or Linux to quickly **build** your apps, **test** them on **Speculos** and **load** them on any supported device.

* Make sure you have installed [Docker](https://www.docker.com/products/docker-desktop/) and it is running.
* Make sure you have an X11 server running.
    * On Ubuntu Linux, it should be running by default.
    * On macOS, install and launch [XQuartz](https://www.xquartz.org/) (make sure to go to XQuartz > Preferences > Security and check "Allow client connections").
    * On Windows, install and launch [VcXsrv](https://sourceforge.net/projects/vcxsrv/)
* Install [VScode](https://code.visualstudio.com/download)
* Open a terminal and clone `app-boilerplate` with `git clone git@github.com:LedgerHQ/app-boilerplate.git`.
* Open the `app-boilerplate` folder with VSCode.
* Open the vscode tasks with  `ctrl + shift + b` (`command + shift + p` on a Mac) and run the following actions :
    * Pull and run the [ledger-app-dev-tools]() docker image by selecting `Run dev-tools image` (select the task for your platform).
    * Build the for the device model of your choice with `Build app`.
    * Test your binary on [Speculos](https://github.com/LedgerHQ/speculos) with `Run Speculos`.

The VsCode tasks also allow you to perform the following actions :

* [Execute functional tests](#test),
* Install functional tests Python requirements (not mandatory because they are already installed in the [ledger-app-dev-tools]()),
* Open a terminal in your docker container,
* Clean build files.
* Load the app on a physical device.

You can find all the available tasks definitions in `.vscode/tasks.json`.

### With a shell and the docker builder image

The app-builder docker image [from this repository](https://github.com/LedgerHQ/ledger-app-builder) contains all needed tools and library to build and load an application.
You can download it from the ghcr.io docker repository:

```shell
sudo docker pull ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder
```

You can then enter this development environment by executing the following command from the directory of the application `git` repository:

```shell
sudo docker run --rm -ti --user "$(id -u)":"$(id -g)" -v "$(realpath .):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder
```

The application's code will be available from inside the docker container, you can proceed to the following compilation steps to build your app.

## Compilation and load

Once you have setup a working development environment (by following either the [VsCode with docker](#with-vscode-and-the-docker-development-tools-image) or [shell with docker](#with-a-shell-and-the-docker-builder-image) approach), you can build and load the app.

Open a terminal of your docker development container and type the following commands :

```shell
make DEBUG=1  # compile optionally with PRINTF
make load     # load the app on the Nano using ledgerblue
```

You can choose which device to compile and load for by setting the `BOLOS_SDK` environment variable to the following values :

* `BOLOS_SDK=$NANOS_SDK`
* `BOLOS_SDK=$NANOX_SDK`
* `BOLOS_SDK=$NANOSP_SDK`
* `BOLOS_SDK=$STAX_SDK`

By default this variable is set to build/load for Nano S.

## Test

The boilerplate app comes with functional tests implemented with Ledger's [Ragger](https://github.com/LedgerHQ/ragger) test framework.

### macOS/Windows

To execute the functional tests on macOS or Windows, it is recommended to use the [VSCode approach](#with-vscode-and-the-docker-development-tools-image) to quickly setup a working test environment.

You can use the following VsCode sequence of tasks (open the menu with `ctrl + shift + b` or `command + shift + p` on a Mac) :

* `Run dev-tools image` (select the task for your platform),
* `Build app` (you will be asked to select a device model),

Then you can select one of those options :

* `Run functional tests` (then select the model you just built the app for).

or simply,

* `Test app with Speculos` (select the correct device model).

### Linux (Ubuntu)

Install the tests requirements :

```shell
pip install -r tests/requirements.txt 
```

Then you can :

* Run the functional tests (here for nanos but available for any device once you have built the binaries) :

```shell
pytest tests/ --tb=short -v --device nanos
```

* Or run your app directly with Speculos

```shell
speculos --model nanos build/nanos/bin/app.elf
```

## Documentation

High level documentation such as [APDU](doc/APDU.md), [commands](doc/COMMANDS.md) and [transaction serialization](doc/TRANSACTION.md) are included in developer documentation which can be generated with [doxygen](https://www.doxygen.nl)

```shell
doxygen .doxygen/Doxyfile
```

the process outputs HTML and LaTeX documentations in `doc/html` and `doc/latex` folders.

## Continuous Integration

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
