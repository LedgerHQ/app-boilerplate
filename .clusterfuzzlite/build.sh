#!/bin/bash -eu

export BOLOS_SDK=/ledger-secure-sdk
export APP_DIR=/app
export APP_FUZZ_SUBDIR=fuzzing
export APP_TARGET=flex
export APP_SANITIZER="${SANITIZER:-address}"

SCRIPT_DIR="${BOLOS_SDK}/fuzzing/scripts"

# shellcheck source=/dev/null
source "${SCRIPT_DIR}/app-common.sh"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/app-config.sh"

BUILD_FAST="${APP_DIR}/${APP_FUZZ_SUBDIR}/build"
INV="${APP_DIR}/${APP_FUZZ_SUBDIR}/invariants/fuzz_globals.zon"
LAYOUT="${APP_DIR}/${APP_FUZZ_SUBDIR}/mock/scenario_layout.h"

echo '.{}' > "${INV}"

rm -rf "${BUILD_FAST}"

configure_fuzz_build "${APP_DIR}" "${BUILD_FAST}" RelWithDebInfo 0

# fuzz_globals must be built first so Absolution emits the generated invariant.
build_fuzzer_target "${BUILD_FAST}" fuzz_globals

INVARIANT_CHANGED=0
sync_invariant "${BUILD_FAST}" fuzz_globals "${INV}"
if [[ "${INVARIANT_CHANGED}" == "1" ]]; then
    build_fuzzer_target "${BUILD_FAST}" fuzz_globals
fi

update_scenario_layout "${BUILD_FAST}" fuzz_globals "${LAYOUT}"

cmake --build "${BUILD_FAST}"

prefix_size="$(prefix_size_from_generated_fuzzer "${BUILD_FAST}" fuzz_globals)"
compat_key="$(python3 "${SCRIPT_DIR}/fuzz_manifest.py" --compat-key "${_APP_MANIFEST}" \
    --prefix-size "${prefix_size}" \
    --invariant "${INV}")"

SEED_CORPUS="${BUILD_FAST}/cfl-seed-corpus"
rm -rf "${SEED_CORPUS}"
mkdir -p "${SEED_CORPUS}"

export BUILD_DIR_FAST="${BUILD_FAST}"
generate_app_seed_corpus "${SEED_CORPUS}" fuzz_globals

BASE_CORPUS="${APP_DIR}/${APP_FUZZ_SUBDIR}/base-corpus"
if [ -d "${BASE_CORPUS}" ]; then
    if [ -f "${BASE_CORPUS}/.compat-key" ]; then
        source_key="$(tr -d '[:space:]' < "${BASE_CORPUS}/.compat-key")"
        if [ -n "${source_key}" ] && [ "${source_key}" = "${compat_key}" ]; then
            echo "Merging compatible base-corpus into fuzz_globals_seed_corpus.zip"
            cp -a "${BASE_CORPUS}/." "${SEED_CORPUS}/"
        else
            echo "Skipping incompatible base-corpus for fuzz_globals_seed_corpus.zip"
            echo "  source compat_key: ${source_key:-<empty>}"
            echo "  current compat_key: ${compat_key}"
        fi
    else
        echo "Skipping base-corpus without .compat-key for fuzz_globals_seed_corpus.zip"
    fi
fi

rm -f "${SEED_CORPUS}/.compat-key"

for fuzzer in "${BUILD_FAST}"/fuzz_*; do
    [[ -f "${fuzzer}" && -x "${fuzzer}" ]] || continue
    cp "${fuzzer}" "${OUT}/"
done

echo "Zipping generated seed corpus into fuzz_globals_seed_corpus.zip"
(cd "${SEED_CORPUS}" && zip -q -r "${OUT}/fuzz_globals_seed_corpus.zip" .)
