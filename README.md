[![Ensure compliance with Ledger guidelines](https://github.com/LedgerHQ/app-boilerplate/actions/workflows/guidelines_enforcer.yml/badge.svg)](https://github.com/LedgerHQ/app-boilerplate/actions/workflows/guidelines_enforcer.yml) [![Build and run functional tests using ragger through reusable workflow](https://github.com/LedgerHQ/app-boilerplate/actions/workflows/build_and_functional_tests.yml/badge.svg?branch=master)](https://github.com/LedgerHQ/app-boilerplate/actions/workflows/build_and_functional_tests.yml)

# Ledger Boilerplate Application

This is a boilerplate application which can be forked to start a new project for the Ledger Nano S/X/SP and Stax.

## Quick start guide

### With VSCode

You can quickly setup a convenient environment to build and test your application by using [Ledger's VSCode developer tools extension](https://marketplace.visualstudio.com/items?itemName=LedgerHQ.ledger-dev-tools) which leverages the [ledger-app-dev-tools](https://github.com/LedgerHQ/ledger-app-builder/pkgs/container/ledger-app-builder%2Fledger-app-dev-tools) docker image.

It will allow you, whether you are developing on macOS, Windows or Linux to quickly **build** your apps, **test** them on **Speculos** and **load** them on any supported device.

* Install and run [Docker](https://www.docker.com/products/docker-desktop/).
* Make sure you have an X11 server running :
    * On Ubuntu Linux, it should be running by default.
    * On macOS, install and launch [XQuartz](https://www.xquartz.org/) (make sure to go to XQuartz > Preferences > Security and check "Allow client connections").
    * On Windows, install and launch [VcXsrv](https://sourceforge.net/projects/vcxsrv/) (make sure to configure it to disable access control).
* Install [VScode](https://code.visualstudio.com/download) and add [Ledger's extension](https://marketplace.visualstudio.com/items?itemName=LedgerHQ.ledger-dev-tools).
* Open a terminal and clone `app-boilerplate` with `git clone git@github.com:LedgerHQ/app-boilerplate.git`.
* Open the `app-boilerplate` folder with VSCode.
* Use Ledger extension's sidebar menu or open the tasks menu with `ctrl + shift + b` (`command + shift + b` on a Mac) to conveniently execute actions :
    * Build the app for the device model of your choice with `Build`.
    * Test your binary on [Speculos](https://github.com/LedgerHQ/speculos) with `Run with Speculos`.
    * You can also run functional tests, load the app on a physical device, and more.

:information_source: The terminal tab of VSCode will show you what commands the extension runs behind the scene.

### With a terminal

The [ledger-app-dev-tools](https://github.com/LedgerHQ/ledger-app-builder/pkgs/container/ledger-app-builder%2Fledger-app-dev-tools) docker image contains all the required tools and libraries to **build**, **test** and **load** an application.

You can download it from the ghcr.io docker repository:

```shell
sudo docker pull ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

You can then enter this development environment by executing the following command from the directory of the application `git` repository:

**Linux (Ubuntu)**

```shell
sudo docker run --rm -ti --user "$(id -u):$(id -g)" --privileged -v "/dev/bus/usb:/dev/bus/usb" -v "$(realpath .):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

**macOS**

```shell
sudo docker run  --rm -ti --user "$(id -u):$(id -g)" --privileged -v "$(pwd -P):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

**Windows (with PowerShell)**

```shell
docker run --rm -ti --privileged -v "$(Get-Location):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

The application's code will be available from inside the docker container, you can proceed to the following compilation steps to build your app.

## Compilation and load

To easily setup a development environment for compilation and loading on a physical device, you can use the [VSCode integration](#with-vscode) whether you are on Linux, macOS or Windows.

If you prefer using a terminal to perform the steps manually, you can use the guide below.

### Compilation

Setup a compilation environment by following the [shell with docker approach](#with-a-terminal).

From inside the container, use the following command to build the app :

```shell
make DEBUG=1  # compile optionally with PRINTF
```

You can choose which device to compile and load for by setting the `BOLOS_SDK` environment variable to the following values :

* `BOLOS_SDK=$NANOS_SDK`
* `BOLOS_SDK=$NANOX_SDK`
* `BOLOS_SDK=$NANOSP_SDK`
* `BOLOS_SDK=$STAX_SDK`

By default this variable is set to build/load for Nano S.

### Loading on a physical device

This step will vary slightly depending on your platform.

:information_source: Your physical device must be connected, unlocked and the screen showing the dashboard (not inside an application).

**Linux (Ubuntu)**

First make sure you have the proper udev rules added on your host :

```shell
# Run these commands on your host, from the app's source folder.
sudo cp .vscode/20-ledger.ledgerblue.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules 
sudo udevadm trigger
```

Then once you have [opened a terminal](#with-a-terminal) in the `app-builder` image and [built the app](#compilation-and-load) for the device you want, run the following command :

```shell
# Run this command from the app-builder container terminal.
make load    # load the app on a Nano S by default
```

[Setting the BOLOS_SDK environment variable](#compilation-and-load) will allow you to load on whichever supported device you want.

**macOS / Windows (with PowerShell)**

:information_source: It is assumed you have [Python](https://www.python.org/downloads/) installed on your computer.

Run these commands on your host from the app's source folder once you have [built the app](#compilation-and-load) for the device you want :

```shell
# Install Python virtualenv
python3 -m pip install virtualenv 
# Create the 'ledger' virtualenv
python3 -m virtualenv ledger
```

Enter the Python virtual environment

* macOS : `source ledger/bin/activate`
* Windows : `.\ledger\Scripts\Activate.ps1`

```shell
# Install Ledgerblue (tool to load the app)
python3 -m pip install ledgerblue 
# Load the app.
python3 -m ledgerblue.runScript --scp --fileName bin/app.apdu --elfFile bin/app.elf
```

## Test

The boilerplate app comes with functional tests implemented with Ledger's [Ragger](https://github.com/LedgerHQ/ragger) test framework.

### macOS / Windows

To test your app on macOS or Windows, it is recommended to use [Ledger's VS Code extension](#with-vscode) to quickly setup a working test environment.

You can use the following sequence of tasks and commands (all accessible in the **extension sidebar menu**) :

* `Select build target`
* `Build app`

Then you can choose to execute the functional tests :

* Use `Run tests`.

Or simply run the app on the Speculos emulator :

* `Run with Speculos`.

### Linux (Ubuntu)

On Linux, you can use [Ledger's VS Code extension](#with-vscode) to run the tests. If you prefer not to, open a terminal and follow the steps below.

Install the tests requirements :

```shell
pip install -r tests/requirements.txt 
```

Then you can :

Run the functional tests (here for nanos but available for any device once you have built the binaries) :

```shell
pytest tests/ --tb=short -v --device nanos
```

Or run your app directly with Speculos

```shell
speculos --model nanos build/nanos/bin/app.elf
```

## Documentation

High level documentation such as [application specification](APP_SPECIFICATION.md), [APDU](doc/APDU.md) and [transaction serialization](doc/TRANSACTION.md) are included in developer documentation which can be generated with [doxygen](https://www.doxygen.nl)

```shell
doxygen .doxygen/Doxyfile
```

the process outputs HTML and LaTeX documentations in `doc/html` and `doc/latex` folders.

## Continuous Integration

The flow processed in [GitHub Actions](https://github.com/features/actions) is the following:

- Ledger guidelines enforcer which verifies that an app is compliant with Ledger guidelines. The successful completion of this reusable workflow is a mandatory step for an app to be available on the Ledger application store. More information on the guidelines can be found in the repository [ledger-app-workflow](https://github.com/LedgerHQ/ledger-app-workflows)
- Code formatting with [clang-format](http://clang.llvm.org/docs/ClangFormat.html)
- Compilation of the application for all Ledger hardware in [ledger-app-builder](https://github.com/LedgerHQ/ledger-app-builder)
- Unit tests of C functions with [cmocka](https://cmocka.org/) (see [unit-tests/](unit-tests/))
- End-to-end tests with [Speculos](https://github.com/LedgerHQ/speculos) emulator and [ragger](https://github.com/LedgerHQ/ragger) (see [tests/](tests/))
- Code coverage with [gcov](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html)/[lcov](http://ltp.sourceforge.net/coverage/lcov.php) and upload to [codecov.io](https://about.codecov.io)
- Documentation generation with [doxygen](https://www.doxygen.nl)

It outputs 3 artifacts:
- `compiled_app_binaries` within binary files of the build process for each device
- `code-coverage` within HTML details of code coverage
- `documentation` within HTML auto-generated documentation

## Are you developing a Nano S, S Plus, X application?

If so, This boilerplate will help you get started.

For a smooth and quick integration:

- See the developers’ documentation on the [Developer Portal](https://developers.ledger.com/), and
- [Go on Discord](https://developers.ledger.com/discord-pro/) to chat with developer support and the developer community.
