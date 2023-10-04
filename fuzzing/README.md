# Fuzzing on transaction parser

## Fuzzing

Fuzzing allows us to test how a program behaves when provided with invalid, unexpected, or random data as input.

In the case of `app-boilerplate` we want to test the code that is responsible for parsing the transaction data, which is `transaction_deserialize()`. To test `transaction_deserialize()`, our fuzz target, `fuzz_tx_parser.c`, needs to implement `int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)`, which provides an array of random bytes that can be used to simulate a serialized transaction. If the application crashes, or a [sanitizer](https://github.com/google/sanitizers) detects any kind of access violation, the fuzzing process is stopped, a report regarding the vulnerability is shown, and the input that triggered the bug is written to disk under the name `crash-*`. The vulnerable input file created can be passed as an argument to the fuzzer to triage the issue.


Note: Usually we want to write a separate fuzz target for each functionality.

## Compilation

In `fuzzing` folder

```
cmake -DBOLOS_SDK=/path/to/sdk -DCMAKE_C_COMPILER=/usr/bin/clang -Bbuild -H.
```

then

```
make -C build
```

## Run

```
./build/fuzz_tx_parser
```
