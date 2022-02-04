# Functional tests

> :point_right: Every path on this document assumes you are at the root of the repository

This directory contains examples of functional tests:

- `tests/ledgercomm/` directory uses the
  [Python LedgerComm library](https://github.com/LedgerHQ/ledgercomm), which
  allows the tests to run either on an actual Nano, or on
  [Speculos](https://github.com/LedgerHQ/speculos),
- `tests/speculos/` directory uses the Python client of
  [Speculos](https://github.com/LedgerHQ/speculos) to run the tests directly on
  the Speculos emulator


## Speculos or LedgerComm?

Speculos is a Nano S/X emulator wich provides a Python HTTP client. Despite not
being able to emulate **every** feature of a Nano S/X, it is a fast and powerful
tool, and is broadly used to test appplication (directly, or through LedgerComm,
or third-part tool such as [Zemu](https://github.com/Zondax/zemu)) which helps
building strong tests and CIs.

LedgerComm is a library which eases the exchange of APDU though HID or TCP
sockets. It works with both a real Nano S/X wallet, or Speculos. As the CI of
this repository does not have access to a Nano S/X, it uses Speculos.

The functional tests using Speculos (`tests/speculos/` directory) or LedgerComm
+ Speculos (`tests/ledgercomm/` directory) are the same, use the same language
(Python) and most of the code is also the same, so it is a good opportunity to
compare how they behave.

Speculos tests are a bit smaller and more straightforward, as the physical
interaction through buttons are managed through the client just like the APDU,
and the emulator is automatically spawned and stopped.

LedgerComm tests are a bit heavier, and need a backend (either Speculos, or a
physical device) up and running, but can be run on an actual Nano S/X.