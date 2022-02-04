# Speculos functional tests

These tests are implemented in Python with the `SpeculosClient` interface which
allows easy execution on the [Speculos](https://github.com/LedgerHQ/speculos) emulator.

## Dependencies

Python dependencies are listed in [requirements.txt](requirements.txt), install them using [pip](https://pypi.org/project/pip/)

```
pip install -r requirements.txt
```

## Launch the tests

Given the requirements are installed, just do:

```
pytest tests/speculos/
```