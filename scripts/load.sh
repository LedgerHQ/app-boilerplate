declare -A options=(["SP"]="\$NANOSP_SDK" ["S"]="\$NANOS_SDK" ["X"]="\$NANOX_SDK" ["STAX"]="\$STAX_SDK" )
docker run --rm -ti  -v "$(realpath .):/app" --privileged -v "/dev/bus/usb:/dev/bus/usb" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest bash -c "BOLOS_SDK=${options[$1]} make load"
