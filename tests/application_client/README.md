# Application Client for Functional Tests

This minimalist Python client is used in the functional tests of the [boilerplate Ledger application](https://github.com/LedgerHQ/app-boilerplate).  
It serves as a communication layer between the test framework (`pytest`) and the device-under-test (Ledger app), sending commands and parsing responses.

## Purpose

This module is not intended to be a full SDK. Instead, it offers just enough abstraction to:

- Send APDUs to the application
- Decode structured responses
- Facilitate clear and maintainable test code

It is intentionally lightweight, focusing on what is strictly necessary to write functional tests.

## When to Use

Use this client as-is when testing the original boilerplate application.  
When you **fork the boilerplate** to implement your own Ledger app, you can **extend or modify this client** to support your custom instruction set, encodings, and behavior.

## Structure

The `application_client` package contains:

- `boilerplate_command_sender.py` — Low-level command encoding and APDU transmission
- `boilerplate_transaction.py` — Helpers to craft and serialize transactions
- `boilerplate_response_unpacker.py` — Functions to decode responses from the app
- `boilerplate_currency_utils.py` — Utility functions for currency-specific formatting
- `boilerplate_utils.py` — Miscellaneous helpers (e.g. encoding, validation)
- `py.typed` — Marker file for type checkers (e.g. `mypy`)

## How to Use

Look at the existing tests for example on how to use this client
