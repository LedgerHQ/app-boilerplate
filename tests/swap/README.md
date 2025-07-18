# Swap Functional Tests

The swap test suite validates the application when it is used by the Exchange application through the os_lib_call mechanism to perform swaps.

This mode uses dedicated launch code and requires a different Speculos setup.

These tests are written using:

- [pytest](https://docs.pytest.org/en/stable/) — Python testing framework
- [Ragger](https://github.com/LedgerHQ/ragger) — Ledger's open-source testing library for simulating device interactions
- The python module [ledger_app_clients.exchange](https://ledgerhq.github.io/app-exchange/) - The helper module to easily emulate the SWAP context.

---

## Purpose

The standalone test suite ensures that the application implements correctly the [swap feature](https://ledgerhq.github.io/app-exchange/)

---

## Directory Structure

```text
swap/
|── cal_helper.py            # A fake CAL configuration for the BOL currency
├── conftest.py              # Pytest fixtures and device setup
├── test_boilerplate.py      # Functional test cases
├── setup_script.py          # A helper script to pull and compile Exchange and Ethereum applications (needed to emulate a swap)
├── snapshots/               # Ragger UI snapshots
├── snapshots-tmp/           # Temporary snapshot diffs (not tracked in git)
├── requirements.txt         # Python dependencies
```
