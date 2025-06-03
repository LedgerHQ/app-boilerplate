#!/bin/bash

set -e

cd ../../

DEVICE="$1"

if [ -z "$DEVICE" ]; then
    echo "Usage: $0 [flex|stax]"
    exit 1
fi

case "$DEVICE" in
    flex)
        SECURE_SDK=/opt/flex-secure-sdk
        ;;
    stax)
        SECURE_SDK=/opt/stax-secure-sdk
        ;;
    *)
        echo "Unsupported device: $DEVICE"
        exit 1
        ;;
esac

echo " ==============================================="
echo " Importing SDK image for ${SECURE_SDK}"
echo " ==============================================="
# Build Docker image
docker build -f fuzzing/macros/src/Dockerfile -t ledger-macro-gen .
# Run Docker container with SDK env variable
docker run --rm -v "$(pwd)":/app -e BOLOS_SDK=${SECURE_SDK} ledger-macro-gen

echo " ==============================================="
python3 fuzzing/macros/src/extract_macros.py --file fuzzing/macros/generated/compile_commands.json --exclude fuzzing/macros/exclude_macros.txt --add fuzzing/macros/add_macros.txt --output fuzzing/macros/generated/macros.txt
echo " ==============================================="
