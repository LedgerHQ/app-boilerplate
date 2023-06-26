# Fuzzing on transaction parser

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
