# Speculos functional tests

> :point_right: Every path on this document assumes you are at the root of the repository.

These tests are implemented in Python with the `SpeculosClient` interface which
allows easy execution on the [Speculos](https://github.com/LedgerHQ/speculos)
emulator.


## Dependencies

Python dependencies are listed in [requirements.txt](requirements.txt), install
them using [pip](https://pypi.org/project/pip/)

```
pip install --extra-index-url https://test.pypi.org/simple/ -r requirements.txt
```

The extra index allows to fetch the latest version of Speculos.


## Launch the tests

Given the requirements are installed, just do:

```
pytest tests/speculos/
```