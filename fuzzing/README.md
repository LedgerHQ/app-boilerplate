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

From the workspace root (or use an absolute `--app-dir`):

```bash
BOLOS_SDK=/path/to/ledger-secure-sdk \
  "$BOLOS_SDK"/fuzzing/scripts/app-campaign.sh \
  --app-dir /path/to/app-boilerplate quick-sanity
```

- **`quick-sanity`** is the **campaign name** (last positional argument, optional).
  Artefacts land in `.fuzz-artifacts/quick-sanity/`. Omit it to use a UTC
  timestamp.
- Default timings are **`WARMUP_SEC=30`** and **`MAIN_SEC=60`** per worker;
  default parallelism is **`WORKERS=min(2, nproc)`** (lightweight for laptops).
- The command builds, syncs the invariant, generates seeds, runs **warmup**
  (broad exploration from bootstrap), then **main** (deeper mutations from the
  merged warmup corpus), and replays the final corpus against a coverage build.

Longer run example:

```bash
WARMUP_SEC=300 MAIN_SEC=3300 WORKERS=4 \
  "$BOLOS_SDK"/fuzzing/scripts/app-campaign.sh \
  --app-dir /path/to/app-boilerplate nightly
```

Chain a prior merged corpus (colon-separated for multiple dirs; each must
match `.compat-key` when that file exists):

```bash
EXTRA_CORPUS=/path/to/app-boilerplate/.fuzz-artifacts/prior/targets/fuzz_globals/corpus \
  "$BOLOS_SDK"/fuzzing/scripts/app-campaign.sh \
  --app-dir /path/to/app-boilerplate follow-up
```

### Useful overrides

| Variable / flag    | Default | Meaning |
|--------------------|---------|---------|
| `WARMUP_SEC`       | `30`    | Warmup seconds **per worker** |
| `MAIN_SEC`         | `60`    | Main phase seconds **per worker** |
| `WORKERS`          | `min(2, nproc)` | Parallel LibFuzzer workers (`1` = minimal CPU) |
| `FUZZ_DEFAULT_WORKERS` | `2` | Cap used when `WORKERS` is unset |
| `EXTRA_CORPUS`     | unset   | Colon-separated extra corpus dirs (bootstrap); see SDK `APP_CONTRACT.md` |
| `BASE_CORPUS_DIR`  | `fuzzing/base-corpus` if present | Promoted seeds; `BASE_CORPUS_DIR=` skips |
| `BUILD_JOBS`       | CPU-based | Parallel compile jobs |
| `OVERWRITE=1`      | unset   | Replace an existing `.fuzz-artifacts/<name>/` |
| `--target NAME`    | all     | Restrict to one fuzzer (here `fuzz_globals`) |
| `--clean`          | off     | Wipe build dirs before configure |

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
