#!/bin/bash

# Defaults
REBUILD=0
COMPUTE_COVERAGE=1
FUZZER=""
RUN_FUZZER=1
TARGET_DEVICE=""
REGENERATE_MACROS=0
BOLOS_SDK=""

# Help message
function show_help() {
    echo "Usage: ./local_run.sh --fuzzer=/path/to/fuzz_dispatcher [--build=1|0] [--compute-coverage=1|0]"
    echo
    echo "  --BOLOS_SDK=PATH            Path to the BOLOS SDK (required)"
    echo "  --TARGET_DEVICE=[flex|stax] Whether it is a flex or stax device"
    echo "  --fuzzer=PATH               Path to the fuzzer binary (required)"
    echo "  --build=1|0                 Whether to build the project (default: 0)"
    echo "  --re-generate-macros=0|1    Whether to regenerate macros or not (default: 0)"
    echo "  --compute-coverage=1|0      Whether to compute coverage after fuzzing (default: 1)"
    echo "  --run-fuzzer=1|0            Whether to run or not the fuzzer (default: 1)"
    echo "  --help                      Show this help message"
    exit 0
}

function gen_macros(){
    # TODO -> remove those lines after installation in the docker image is done by default
    apt-get update && apt-get install -y bear
    cd ..
    case "$TARGET_DEVICE" in
        flex)
            make clean BOLOS_SDK=/opt/flex-secure-sdk
            bear --output fuzzing/macros/generated/used_macros.json -- make DEBUG=1 BOLOS_SDK=/opt/flex-secure-sdk
            ;;
        stax)
            make clean BOLOS_SDK=/opt/stax-secure-sdk
            bear --output fuzzing/macros/generated/used_macros.json -- make DEBUG=1 BOLOS_SDK=/opt/stax-secure-sdk
            ;;
        *)
            echo "Unsupported device: $TARGET_DEVICE"
            exit 1
            ;;
    esac
    cd fuzzing/macros/ || exit
    python3 extract_macros.py --file generated/used_macros.json --exclude exclude_macros.txt --add add_macros.txt --output generated/macros.txt
    cd ..
}

function build(){
    # TODO -> remove those lines after installation in the docker image is done by default
    apt update && apt install -y libclang-rt-dev ninja-build
    
    rm -rf build
    cmake -S . -B build -DCMAKE_C_COMPILER=clang -DSANITIZER=address -DTARGET_DEVICE="$TARGET_DEVICE" -DBOLOS_SDK="$BOLOS_SDK" -G Ninja
    cmake --build build
}

# Parse args
for arg in "$@"; do
    case $arg in
        --fuzzer=*)
            FUZZER="${arg#*=}"
            ;;
        --BOLOS_SDK=*)
            BOLOS_SDK="${arg#*=}"
            ;;
        --TARGET_DEVICE=*)
            TARGET_DEVICE="${arg#*=}"
            ;;
        --re-generate-macros=*)
            REGENERATE_MACROS="${arg#*=}"
            ;;
        --build=*)
            REBUILD="${arg#*=}"
            ;;
        --compute-coverage=*)
            COMPUTE_COVERAGE="${arg#*=}"
            ;;
        --run-fuzzer=*)
            RUN_FUZZER="${arg#*=}"
            ;;
        --help)
            show_help
            ;;
        *)
            echo "Unknown argument: $arg"
            show_help
            ;;
    esac
done


# Validate required args
if [ -z "$BOLOS_SDK" ]; then
    echo "Error: --BOLOS_SDK=$BOLOS_SDK is required."
    show_help
    # shellcheck disable=SC2317
    exit 1
fi

if [ "$TARGET_DEVICE" != "flex" ] && [ "$TARGET_DEVICE" != "stax" ]; then
    echo "TARGET_DEVICE=$TARGET_DEVICE not recognized, use flex | stax"
    exit 1
fi
if [ "$REGENERATE_MACROS" -ne 0 ]; then
    gen_macros
fi
if [ "$REBUILD" -eq 1 ]; then
    build

    echo ""
    echo "----------"
    echo "Info: You have a fuzzer now. Run: ./local_run.sh --fuzzer=[PATH-TO-FUZZER] --compute-coverage=1"
    echo "Tip: Fuzzers will be in build/fuzz_* :"
    ls build/fuzz*
    echo "----------"
fi
if { [ -z "$FUZZER" ] || [ ! -x "$FUZZER" ]; } then
    echo ""
    echo "Given fuzzer '$FUZZER' is not executable or was not set."
    echo ""
    exit 1
fi

if ! [ -d ./out ]; then
    mkdir out
fi

if ! [ -d ./out/corpus ]; then
    mkdir out/corpus
fi

if [ "$COMPUTE_COVERAGE" -ne 1 ]; then
    echo ""
    echo "----------"
    echo "Info: Fuzzer will start soon, but coverage will not be computed since --compute-coverage=0"
    echo "----------"
    echo ""
fi

if [ "$RUN_FUZZER" -eq 1 ]; then
    # Run fuzzer
    ncpus=$(nproc)
    jobs=$((ncpus / 2))
    echo ""
    echo "----------"
    echo "Info: Starting fuzzer... Press Ctrl-C to stop."
    echo "----------"
    echo ""
    "$FUZZER" -max_len=8192 -jobs="$jobs" -timeout=10 ./out/corpus
fi

# Exit early if coverage isn't required
if [ "$COMPUTE_COVERAGE" -ne 1 ]; then
    mv -- *.log *.profraw out/ 2>/dev/null
    echo ""
    echo "----------"
    echo "Info: Generated data moved to out folder"
    echo "----------"
    exit 0
fi

# Compute coverage
echo "----------"
echo "Info: Computing coverage..."
echo "----------"

rm -f out/default.profdata out/default.profraw
"$FUZZER" -max_len=8192 -runs=0 ./out/corpus

mv -- *.log *.profraw out/ 2>/dev/null
llvm-profdata merge -sparse out/*.profraw -o out/default.profdata
llvm-cov show "$FUZZER" --ignore-filename-regex="$BOLOS_SDK" -instr-profile=out/default.profdata -format=html -output-dir=out/
llvm-cov report --ignore-filename-regex="$BOLOS_SDK" "$FUZZER" -instr-profile=out/default.profdata

echo ""
echo "----------"
echo "Generated data moved to out folder"
echo "To see code coverage in the web run (in another terminal):"
echo "xdg-open out/report.html"
echo "----------"
echo ""
