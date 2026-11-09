# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.4.0] - 2026-11-09

### Added

- Transaction review displays the memo and the token contract address
- Swap coin configuration can end with the token contract address, checked against the signed transaction

### Changed

- A memo must be printable ASCII, checked with the SDK `is_printable_string()`
- In swap mode, the transaction memo must match the destination extra id
- Claude workflow restricted to repository owners, members and collaborators

### Fixed

- Out-of-bounds read when checking the blind-sign memo
- Sanitizer flags applied to the fuzzed transaction parser
- Swap helper tool runs its commands without a shell
- Standalone test instructions no longer use TestPyPI nor `--privileged` to compile

## [2.1.0] - 2023-10-06

### Changed

- Improving the settings use case in order to be able to use app settings parameters stored in NVM
- add a NBGL use case choice when a setting switch is toggled

## [2.0.0] - 2023-07-10

### Added

- Stax porting
- Extensive CI, including mandatory `guidelines_enforcer.yml`
- Extensive `README.md` to modify/compile/test the application on most OS (Linux, MacOS, Windows)
- Extensive `Ragger` tests

### Changed

- Simplified `Makefile` (complexity delegated to the SDK's `Makefile.standard_app`)
- Simplified overall code (moved into the SDK)
- Improving several UI flows to fit Ledger UI guidelines
- Removing `TRY`/`CATCH` usage (using `_no_throw` SDK functions)
- Cleaning unnecessary resources (moved into the SDK)

### Fixed

- Multiple minor lint, prototype or misspell fixes

## [1.0.1] - 2021-01-11

### Fix

- Missing header includes

## [1.0.0] - 2020-11-19

### Added

- Initial commit with the brand new Boilerplate application
