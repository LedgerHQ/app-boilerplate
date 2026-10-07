### The Swap functional tests setup

The swap test suite validates the application when it is used by the Exchange application
through the os_lib_call mechanism to perform the [swap feature](https://ledgerhq.github.io/app-exchange/).

This mode uses dedicated launch code and requires a different Speculos setup.

These tests are written using:

- [pytest](https://docs.pytest.org/en/stable/) — Python testing framework
- [Ragger](https://github.com/LedgerHQ/ragger) — Ledger's open-source testing library for simulating device interactions
- The python module [ledger_app_clients.exchange](https://ledgerhq.github.io/app-exchange/),
  the helper module to easily emulate the SWAP context.

Directory structure brief overview:

```text
swap/
|── cal_helper.py                     # A fake CAL configuration for the BOL currency
├── conftest.py                       # Pytest fixtures and device setup
├── test_boilerplate.py               # Functional test cases
├── snapshots/                        # Ragger UI snapshots
├── snapshots-tmp/                    # Temporary snapshot diffs (not tracked in git)
├── requirements.txt                  # Python dependencies
```

### Compile your application

Build your application in the **Ledger docker** environment.

### Fetch the Exchange and Ethereum applications

Execute the fetch script in your **native** (host) environment or in the **Ledger docker** environment.
It downloads the prebuilt test binaries of the applications listed in `ledger_app.toml` (`[pytest.swap.dependencies]`)
and puts them in `.test_dependencies/`. Nothing is compiled.

```sh
python tests/swap/helper_tool_fetch_dependencies.py
```

For private repositories, set `GH_TOKEN` or log in with `gh auth login`.

## Running a first Exchange test

If you are on Linux or WSL, you can run the tests in your **native** (host) environment.

If you are on MAC you need to run the tests in the **Ledger docker** environment.

### Install python dependencies

Install python dependencies

```sh
pip install -r tests/swap/requirements.txt --break-system-packages
```

### Run a simple test

To see all available tests:

```sh
pytest -v --tb=short tests/swap/ --device all --collect-only
```

To list only SWAP tests for Stax:

```sh
pytest -v --tb=short tests/swap/ --device stax --collect-only -k swap
```

To run a specific test for Stax:

```sh
pytest -v --tb=short tests/swap/ --device stax -k 'swap_ui_only' -s
```

Running a single test will be useful later for test driven development,
don't hesitate to come back to this command and adapt it.
