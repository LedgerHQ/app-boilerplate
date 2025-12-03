#!/bin/bash -eu

# build fuzzers

pushd fuzzing
rm -rf build
cmake -DBOLOS_SDK=../BOLOS_SDK -Bbuild -H.
make -C build
# Copy all fuzz harnesses to output directory
mv ./build/fuzz_* "${OUT}"
popd
