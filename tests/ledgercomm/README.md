# LedgerComm functional tests

> :point_right: Every path on this document assumes you are at the root of the repository.

These tests are implemented in Python and can be executed either using the
[Speculos](https://github.com/LedgerHQ/speculos) emulator or a Ledger Nano S/X.

Python dependencies are listed in [requirements.txt](requirements.txt), install
them using [pip](https://pypi.org/project/pip/)

```
pip install -r tests/ledgercomm/requirements.txt
```


## Launch with Speculos

You will need Speculos installed first:

```
pip install --extra-index-url https://test.pypi.org/simple/ speculos
```

The extra index allows to fetch the latest version of Speculos.


Then, start the boilerplate application on Speculos:

```
speculos bin/app.elf --button-port 42000
```

then in the `tests` folder run

```
pytest --headless tests/ledgercomm/
```

The `--headless` option means the tests will trigger the buttons themselves
(button actions are needed to approve the signing), by connecting on the `42000`
port opened by Speculos (`--button-port 42000`).

By removing this option, the tests will be stuck, waiting for the someone to
approve the step. The user could validate this step manually, and the test will
continue thereafter.


## Launch with your Nano S/X

To run the tests on your Ledger Nano S/X, you also need to install an optional
dependency:

```
pip install ledgercomm[hid]
```

Be sure to have the boilerplate application installed and opened on the device,
and the device connected through USB, without any other software interacting
with it. Then run:

```
pytest --hid tests/ledgercomm/
```
