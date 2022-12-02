# Ragger functional tests

> :point_right: Every path on this document assumes you are at the root of the
repository.

These tests are implemented in Python with the `Ragger` library, allowing to
write tests which will run on both the
[Speculos](https://github.com/LedgerHQ/speculos) emulator and physical devices
by using either [LedgerComm](https://github.com/LedgerHQ/ledgercomm) or
[LedgerWallet](https://github.com/LedgerHQ/ledgerctl) libraries.


## Dependencies

Python dependencies are listed in [requirements.txt](requirements.txt), install
them using [pip](https://pypi.org/project/pip/)

```
pip install --extra-index-url https://test.pypi.org/simple/ -r requirements.txt
```

The extra index allows to fetch the latest version of Ragger.


## Launching the tests

### On Speculos

Given the requirements are installed, you need to generate the application binaries
for all the SDK. The tests will expect the binaries to be:

- `elfs/boilerplate_nanos.elf`
- `elfs/boilerplate_nanox.elf`
- `elfs/boilerplate_nanosp.elf`

Once that's done, just do:

```
pytest -v tests/ragger/
```

This will run the tests inside the Speculos emulator, for all the current device
SDKs (NanoS, NanoX and NanoS+).

It is possible to test on a specific device model only:

```
pytest --nanox tests/ragger/
```

### On a physical device

To run the tests on a physical device:

- Install boilerplate on the device and start the application;
- Comment the `conftest.py` file to remove the undesired devices in the `APPS`
  list, so that only one device matching your physical device remains;
- Launch the tests with the backend of your choice (they should be equivalent):

```
pytest tests/ragger/ --backend ledgerwallet
```
... or:

```
pytest tests/ragger/ --backend ledgercomm
```
