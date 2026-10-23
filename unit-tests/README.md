# Unit tests

The unit tests use the Unity/CMock framework of the SDK, `cmake/LedgerUT.cmake`, documented in
[cmake/USAGE.md](https://github.com/LedgerHQ/ledger-secure-sdk/blob/master/cmake/USAGE.md).

## Run the tests

`BOLOS_SDK` must point at the SDK checkout; it is already set in the app builder images.

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure
```

Every test is built with AddressSanitizer and UndefinedBehaviorSanitizer. Configure with
`-DLEDGER_UT_SANITIZERS=OFF` to build without them.

## Coverage

After running the tests:

```bash
cmake --build build --target generate_coverage
```

The HTML report is written to `build/coverage/`.

## Add a test

Write `test_<name>.c` in this directory, then declare it in [CMakeLists.txt](CMakeLists.txt) with
`ledger_unit_tests_add_test()`, listing the sources under test and the headers to mock.
