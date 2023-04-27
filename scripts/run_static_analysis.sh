declare -A options=(["SP"]="\$NANOSP_SDK" ["S"]="\$NANOS_SDK" ["X"]="\$NANOX_SDK" ["STAX"]="\$STAX_SDK" )
docker run --rm -ti  -v "$(realpath .):/app" --privileged -v "/dev/bus/usb:/dev/bus/usb" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest bash -c "scan-build --use-cc=clang -analyze-headers -enable-checker security -enable-checker unix -enable-checker valist -o scan-build --status-bugs make -j ENABLE_SDK_WERROR=1 BOLOS_SDK=${options[$1]}"
