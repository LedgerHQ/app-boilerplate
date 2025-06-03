#!/bin/bash -eu

# Handle optional BOLOS_SDK argument like: ./script.sh BOLOS_SDK=/path/to/sdk
for arg in "$@"; do
    case $arg in
        BOLOS_SDK=*)
            export BOLOS_SDK="${arg#*=}"
            ;;
    esac
done

# Fall back to environment variable if not passed as argument
: "${BOLOS_SDK:?BOLOS_SDK must be provided as env var or argument like BOLOS_SDK=/path}"

cd /app

# Clean or create build directory
if [ -d "build" ]; then
    rm -r build
fi
mkdir -p build

echo " ==============================================="
echo " Generating MACROS for ${BOLOS_SDK}"
echo " ==============================================="

# Build using bear
bear -- make DEBUG=1

# Move the generated compile_commands.json
mv compile_commands.json fuzzing/macros/generated/compile_commands.json
