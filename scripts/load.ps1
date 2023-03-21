param(
[string]$arg1
)
usbipd wsl attach --busid "1-3"
docker run --rm -ti  -v "$(pwd):/app:rw" --privileged -v "/dev/bus/usb:/dev/bus/usb" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest bash -c "sh scripts/load_setup.sh $arg1"
usbipd wsl detach --busid "1-3"