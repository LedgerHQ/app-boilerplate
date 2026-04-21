# Boilerplate Fuzzing

Coverage-guided fuzzing for the boilerplate app. Two tools do the heavy lifting:

- **Ledger Secure SDK — fuzzing framework**: builds a sanitizer-instrumented
  LibFuzzer binary from the app sources and runs campaigns with a standard
  layout (warmup + main + coverage replay).
- **Absolution**: turns the first bytes of each input into app globals
  (state, BIP32 path, swap mode, …) so the fuzzer explores meaningful
  combinations of state instead of random garbage.

Everything app-specific lives in this folder; the framework does the rest.

## Prerequisites

- `BOLOS_SDK` set to a checkout of the Ledger Secure SDK that contains the
  fuzzing framework.
- Absolution installed (see `$BOLOS_SDK/fuzzing/README.md`).
- Clang ≥ 14 with `llvm-profdata` and `llvm-cov` for coverage reports.

## Run a campaign

```bash
WARMUP_SEC=30 MAIN_SEC=60 \
  "$BOLOS_SDK"/fuzzing/scripts/app-campaign.sh \
  --app-dir app-boilerplate quick-sanity
```

- The trailing positional argument (`quick-sanity`) is the campaign name and
  becomes the output directory. Omit it to default to a UTC timestamp.
- The command builds, syncs the invariant, generates seeds, runs a short
  **warmup** (wide coverage fast), a longer **main** phase (depth-first
  exploration from the warmup corpus), and replays the final corpus against
  a coverage build.

### Useful overrides

| Variable / flag    | Default | Meaning                                             |
|--------------------|---------|-----------------------------------------------------|
| `WARMUP_SEC`       | 120     | warmup phase duration per worker                    |
| `MAIN_SEC`         | 900     | main phase duration per worker                      |
| `WORKERS`          | `nproc` | parallel LibFuzzer workers                          |
| `OVERWRITE=1`      | unset   | reuse an existing campaign directory                |
| `--target NAME`    | all     | restrict to one fuzzer (here `fuzz_globals`)        |
| `--clean`          | off     | wipe `build/` before configuring                    |

## What you get

Each run writes to `.fuzz-artifacts/<campaign-name>/` (gitignored):

- `targets/fuzz_globals/bootstrap-base/` — seed corpus from dictionary + manifest
- `targets/fuzz_globals/warmup/`, `warmup-merged/`, `main/` — per-worker corpora
- `targets/fuzz_globals/meta.env`, `fuzz_globals.dict` — run metadata
- `report/index.html` — LLVM source-level coverage report

Crashes, if any, land as `crash-*` files under the worker directories and
are summarised at the end of the run.

## Files in this folder

| Path                              | Purpose                                                                 |
|-----------------------------------|-------------------------------------------------------------------------|
| `CMakeLists.txt`                  | `ledger_fuzz_setup()` + `ledger_fuzz_add_app_target(fuzz_globals)`      |
| `fuzz-manifest.toml`              | coverage key files, dictionary, seed strategy                           |
| `harness/fuzz_dispatcher.c`       | APDU dispatcher on top of `fuzz_harness_entry()` + swap callback lane   |
| `mock/mocks.c` / `mock/mocks.h`   | app-side framework globals and the BSS-zero no-op                       |
| `mock/scenario_layout.h`          | prefix offsets, auto-synced by the framework                            |
| `invariants/fuzz_globals.zon`     | Absolution invariant (app state model, auto-synced)                     |
| `invariants/zero-symbols.txt`     | app globals stripped from the prefix                                    |
| `invariants/domain-overrides.txt` | enum/state constraints that improve convergence                         |
| `macros/add_macros.txt`           | extra compile definitions added on top of the app `Makefile` defines    |
| `macros/exclude_macros.txt`       | compile definitions removed from the fuzz build                         |
