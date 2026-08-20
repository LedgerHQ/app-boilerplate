# Boilerplate Fuzzing

Coverage-guided fuzzing for the boilerplate app, on the SDK fuzzing framework.
One APDU per iteration through `apdu_dispatcher()`; app state is restored from
the input prefix by Absolution, so the harness has no per-iteration setup.

**This folder is the template.** The SDK ships no separate app skeleton — copy
`fuzzing/` into your app and adapt the four files below. Concepts, commands and
the full schema live in `$BOLOS_SDK/fuzzing/doc/` (`manifest.dox`,
`invariants.dox`, `mocks.dox`, `running.dox`, `ci.dox`). For targets that fuzz
the SDK itself rather than an app, see `fuzzing/sdk-fuzz/` in the SDK.

## Quickstart

```bash
export BOLOS_SDK=/absolute/path/to/ledger-secure-sdk

BOLOS_SDK="$BOLOS_SDK" FUZZ_TIME=0 \
  "$BOLOS_SDK"/fuzzing/scripts/app-campaign.sh --clean --app-dir "$(pwd)" probe-build

BOLOS_SDK="$BOLOS_SDK" OVERWRITE=1 FUZZ_TIME=300 APP_SANITIZER=address \
  "$BOLOS_SDK"/fuzzing/scripts/app-campaign.sh --app-dir "$(pwd)" baseline
```

`FUZZ_TIME` is per worker. `APP_SANITIZER` takes `address`, `undefined` or
`memory`; the unprefixed `SANITIZER` is ignored. Pass `--clean` when switching
sanitizer, or the previous `-fsanitize=` persists in the CMake cache. Artifacts
land in `.fuzz-artifacts/<run-name>/`.

## What an app has to provide

Everything else is in the SDK. The whole app-side contract is four files:

```text
fuzzing/
  CMakeLists.txt             sources, include dirs, target name
  fuzz-manifest.toml         target, coverage key files, dictionary, seeds
  harness/fuzz_dispatcher.c  command table + fuzz_app_dispatch()
  invariants/                zero-symbols.txt, domain-overrides.txt
```

- **`fuzz_commands[]`** maps each INS to its CLA, P1/P2 range and whether it
  carries data. `FUZZ_COMMAND_COUNT()` derives the count.
- **`fuzz_app_dispatch()`** hands the framework's `command_t` to the app's real
  dispatcher. It may adapt shape — here P2 carries a flag, so its two clamped
  values are mapped onto the two the dispatcher accepts — but it must never
  author content.
- **`fuzz_app_reset()`** is optional; the SDK ships a weak no-op. Define it only
  if the app needs per-iteration setup the invariant cannot express.
- **`fuzz_entry()`** is optional too, via `FUZZ_APP_CUSTOM_ENTRY`. This app
  defines one to add a lane for the swap library callbacks, which the Exchange
  app enters directly and no APDU can reach.

Macros, mocks, the campaign scripts, coverage, the corpus format and the TLV
mutator all come from the SDK. `HAVE_*`, `APPNAME` and the rest arrive
transitively from `secure_sdk` (`make list-defines` plus
`macros/add_macros.txt`), so they do not belong in `COMPILE_DEFINITIONS`.

`invariants/fuzz_globals.zon` is generated per build and gitignored: it records
absolute source paths, which would tie the corpus compat key to one checkout.

## Writing invariants

A prefix byte selects **by index** into a domain's value list, so the first value
listed is what a zero prefix restores — put the productive value first when a
zero would make the app reject before reaching anything.

Constrain restored **state** to values a real execution can produce; never
constrain restored **content**. `G_swap_response_ready` is the worked example:
`lib_standard_app/main.c` clears it at startup and is excluded from this build,
so leaving it fuzzable restores a state production cannot present, and the app
correctly treats it as a broken invariant.

Dictionary entries earn their place by being bytes the fuzzer cannot reach on
its own. CLA, INS, P1 and P2 never appear in the tail — the framework writes
them from `fuzz_commands[]` — so tokens for them can never match.
