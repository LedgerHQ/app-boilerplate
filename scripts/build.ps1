param(
[string]$arg1
)
docker run --rm -ti  -v "$(pwd):/app:rw" --privileged -v "/dev/bus/usb:/dev/bus/usb" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest bash -c "sh scripts/build_setup.sh $arg1"
