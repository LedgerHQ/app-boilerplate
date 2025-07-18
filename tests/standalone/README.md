# Standalone Functional Tests

This directory contains the **standalone functional test suite** for the Ledger application.  
It is intended to validate the application’s behavior in a **generic context**, when launched directly from the device's dashboard.

These tests are written using:

- [pytest](https://docs.pytest.org/en/stable/) — Python testing framework
- [Ragger](https://github.com/LedgerHQ/ragger) — Ledger's open-source testing library for simulating device interactions

---

## Purpose

The standalone test suite ensures that:

- The application launches correctly from the dashboard
- The main menu and navigation behave as expected
- Core commands (e.g., `GET_VERSION`, `GET_PUBLIC_KEY`, `SIGN_TX`) function properly
- User approval flows work under normal conditions
- Errors are correctly reported and handled

---

## Directory Structure

```text
standalone/
├── conftest.py              # Pytest fixtures and device setup
├── test_*.py                # Functional test cases
├── snapshots/               # Ragger UI snapshots
├── snapshots-tmp/           # Temporary snapshot diffs (not tracked in git)
├── requirements.txt         # Python dependencies
└── utils.py                 # Local test helpers
```
