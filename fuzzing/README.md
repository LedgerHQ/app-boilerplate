# Fuzzing Harnesses for Cardano Ledger App

## Overview

Fuzzing allows us to test how a program behaves when provided with invalid, unexpected, or random data as input.

This directory contains multiple fuzzing harnesses for security-critical components of the Cardano app:
- **`fuzz_signOpCert`** - Tests operational certificate signing
- **`fuzz_getPublicKeys`** - Tests BIP44 path parsing and key derivation
- **`fuzz_all_handlers`** - Tests APDU dispatcher and command routing

Each harness implements `int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)`, which feeds random APDU-formatted data to the handlers.

## Available Harnesses

### fuzz_signOpCert
Tests the operational certificate signing handler for robustness against:
- Malformed KES public keys
- Invalid path specifications
- Boundary conditions in period/counter values

### fuzz_getPublicKeys
Tests public key derivation for:
- Invalid BIP44 paths
- Buffer overflow attempts
- Path validation enforcement

### fuzz_all_handlers
Tests the main APDU dispatcher by:
- Routing random command sequences
- Testing state machine consistency
- Checking instruction validation and routing

## Building and Running Fuzzers

### Local Build (Recommended for Development)

The fuzzing harnesses can be built locally using Clang and the Ledger SDK:

```bash
cd fuzzing
cmake -DBOLOS_SDK=/opt/ledger-secure-sdk -DCMAKE_C_COMPILER=/usr/bin/clang -Bbuild -H.
make -C build
```

**What this does:**
1. Configures the build with the Ledger SDK path
2. Compiles all fuzzing harnesses with address sanitizer and libfuzzer
3. Output binaries: `build/fuzz_signOpCert`, `build/fuzz_getPublicKeys`, `build/fuzz_all_handlers`

### Container-Based Build (For CI/Continuous Fuzzing)

For continuous fuzzing integration with OSS-Fuzz:

```bash
mkdir -p fuzzing/out
docker build -t cardano-app --file .clusterfuzzlite/Dockerfile .
docker run --rm --privileged -e FUZZING_LANGUAGE=c \
    -v "$(realpath .)/fuzzing/out:/out" -ti cardano-app
```

**What happens:**
1. `ledger-app-builder-lite` builds the BOLOS SDK
2. `oss-fuzz-base/base-builder` provides clang, libfuzzer, and sanitizers
3. `build.sh` compiles all fuzzing harnesses
4. Output binaries in `fuzzing/out/`: `fuzz_signOpCert`, `fuzz_getPublicKeys`, `fuzz_all_handlers`

### Running Fuzzers

After local build:

```bash
cd fuzzing

# Interactive fuzzing with seed corpus
./build/fuzz_signOpCert ./corpus

# Fuzzing without seed (finds more edge cases, slower startup)
./build/fuzz_getPublicKeys

# Test multi-command sequences
./build/fuzz_all_handlers ./corpus -max_len=8192
```

After container build, binaries will be in `out/` instead of `build/`.

### Crash Reproduction

If fuzzing finds a crash, it will save the input to `crash-*`:

```bash
# Reproduce a specific crash
./out/fuzz_signOpCert crash-abc123
```

## Continuous Fuzzing via Google OSS-Fuzz

For production continuous fuzzing integration with Google's OSS-Fuzz infrastructure:

The repository includes `.clusterfuzzlite/` configuration files that enable automatic fuzzing campaigns.

**How it works:**
1. `.clusterfuzzlite/Dockerfile` - Multi-stage build:
   - Stage 1: `ledger-app-builder-lite` compiles BOLOS SDK
   - Stage 2: `oss-fuzz-base/base-builder` provides fuzzing infrastructure
2. `.clusterfuzzlite/build.sh` - Build script that:
   - Calls `cmake -DBOLOS_SDK=../BOLOS_SDK`
   - Compiles all fuzz harnesses
   - Outputs binaries to `$OUT` directory

**Integration:**
- This setup is compatible with Google's OSS-Fuzz and ClusterFuzzLite services
- No additional configuration needed beyond what's in `.clusterfuzzlite/`
- See [Google OSS-Fuzz documentation](https://google.github.io/oss-fuzz/) for integration details

## Corpus Seed Data

The `corpus/` directory contains minimal seed inputs to accelerate fuzzing:
- `signOpCert_basic` - Valid operational certificate data
- `getPublicKeys_bip44_mainnet` - BIP44 mainnet path
- `allHandlers_sequence` - Multi-command APDU sequence
- Edge case files for testing error handling

## Notes

- Fuzzing requires **Clang** compiler
- Address sanitizer catches memory errors automatically
- Coverage mapping tracks which code paths are tested
- Long-running fuzzing campaigns may find subtle bugs
- Corpus files should be added as new interesting inputs are discovered

## References

- [Google Sanitizers](https://github.com/google/sanitizers)
- [LLVM LibFuzzer](https://llvm.org/docs/LibFuzzer/)
- [ClusterFuzzLite](https://google.github.io/clusterfuzzlite/)
- [OSS-Fuzz](https://google.github.io/oss-fuzz/)
