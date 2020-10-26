# Ledger Boilerplate Application

This is a boilerplate application which can be forked to start a new project for the Ledger Nano S/X.

## Prerequisite

Be sure to have your environment correctly set up (see [Getting Started](https://ledger.readthedocs.io/en/latest/userspace/getting_started.html)) and [ledgerblue](https://pypi.org/project/ledgerblue/) and installed.

## Compilation

```
make DEBUG=1  # compile with PRINTF
make load     # load the app on the Nano using ledgerblue 
```

## Tests

End-to-end tests are implemented in Python and can be executed either using the [Speculos](https://github.com/LedgerHQ/speculos) emulator or a Ledger Nano S/X.
Python dependencies required are [pytest](https://pypi.org/project/pytest/) and [ledgercomm](https://pypi.org/project/ledgercomm/), install them using [pip](https://pypi.org/project/pip/)

```
pip install -r tests/requirements.txt
```

### Launch with Speculos

First start your application with Speculos

```
./speculos.py /path/to/app-boilerplate/bin/elf --ontop --sdk 1.6
```

then at the root of `app-boilerplate` folder run

```
pytest
```

### Launch with your Nano S/X

Be sure to have you device connected through USB (without any other software interacting with it) and run

```
pytest --hid
```
