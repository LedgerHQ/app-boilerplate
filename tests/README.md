# End-to-end tests

These tests are implemented in Python and can be executed either using the [Speculos](https://github.com/LedgerHQ/speculos) emulator or a Ledger Nano S/X.
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

To run the tests on your Ledger Nano S/X you also need to install optional dependency

```
pip install ledgercomm[hid]
```

Be sure to have you device connected through USB (without any other software interacting with it) and run

```
pytest --hid
```
