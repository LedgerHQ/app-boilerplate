#!/bin/bash -eu

# build fuzzers

pushd fuzzing
cmake -Bbuild -H.
make -C build
mv ./build/fuzz_tx_parser $OUT
popd

