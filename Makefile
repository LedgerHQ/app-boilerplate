# ****************************************************************************
#    Ledger App Boilerplate
#    (c) 2020 Ledger SAS.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# ****************************************************************************

ifeq ($(BOLOS_SDK),)
$(error Environment variable BOLOS_SDK is not set)
endif

$(shell rm -rf ./public_sdk)
$(shell cp -r $(BOLOS_SDK) .)
$(shell rm -rf ./public_sdk/lib_stusb)
$(shell rm -rf ./public_sdk/lib_stusb_impl)
$(shell rm -rf ./public_sdk/lib_blewbxx)
$(shell rm -rf ./public_sdk/lib_blewbxx_impl)
$(shell rm -rf ./public_sdk/src/os_io_task.c)
$(shell rm -rf ./public_sdk/src/os_io_seproxyhal.c)
$(shell rm -rf ./public_sdk/include/os_io_seproxyhal.h)
$(shell rm -rf ./public_sdk/src/ledger_protocol.c)
$(shell rm -rf ./public_sdk/include/ledger_protocol.h)
$(shell rm -rf ./public_sdk/include/os_io.h)
$(shell cp -r ./sdk_patch/lib_usb ./public_sdk/)
$(shell cp -r ./sdk_patch/lib_ble ./public_sdk/)
$(shell cp -r ./sdk_patch/protocol ./public_sdk/)
$(shell cp ./sdk_patch/os_io_task.c ./public_sdk/src/)
$(shell cp ./sdk_patch/os_io_seproxyhal.c ./public_sdk/src/)
$(shell cp ./sdk_patch/os_io_seproxyhal.h ./public_sdk/include/)
$(shell cp ./sdk_patch/os_io.h ./public_sdk/include/)

BOLOS_SDK = ./public_sdk

include $(BOLOS_SDK)/Makefile.defines

APP_LOAD_PARAMS  = --curve secp256k1
ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_NANOX TARGET_STAX TARGET_FLEX TARGET_APEX_P))
APP_LOAD_PARAMS += --appFlags 0x200  # APPLICATION_FLAG_BOLOS_SETTINGS
else
APP_LOAD_PARAMS += --appFlags 0x000
endif
APP_LOAD_PARAMS += --path "44'/1'"   # purpose=coin(44) / coin_type=Testnet(1)
APP_LOAD_PARAMS += $(COMMON_LOAD_PARAMS)

APPNAME      = "Boilerplate"
APPVERSION_M = 1
APPVERSION_N = 0
APPVERSION_P = 1
APPVERSION   = "$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)"

ifeq ($(TARGET_NAME),TARGET_NANOS)
    ICONNAME=icons/nanos_app_boilerplate.gif
else ifeq ($(TARGET_NAME),TARGET_STAX)
    ICONNAME=icons/stax_app_boilerplate_32px.gif
else ifeq ($(TARGET_NAME),TARGET_FLEX)
    ICONNAME=icons/stax_app_boilerplate_32px.gif
else
    ICONNAME=icons/nanox_app_boilerplate.gif
endif

DEFINES += $(DEFINES_LIB)
DEFINES += APPNAME=\"$(APPNAME)\"
DEFINES += APPVERSION=\"$(APPVERSION)\"
DEFINES += MAJOR_VERSION=$(APPVERSION_M) MINOR_VERSION=$(APPVERSION_N) PATCH_VERSION=$(APPVERSION_P)
DEFINES += OS_IO_SEPROXYHAL
DEFINES += HAVE_SPRINTF HAVE_SNPRINTF_FORMAT_U
DEFINES += HAVE_IO_USB HAVE_L4_USBLIB IO_USB_MAX_ENDPOINTS=6 IO_HID_EP_LENGTH=64 HAVE_USB_APDU
DEFINES += USB_SEGMENT_SIZE=64
DEFINES += BLE_SEGMENT_SIZE=32
DEFINES += HAVE_WEBUSB WEBUSB_URL_SIZE_B=0 WEBUSB_URL=""
DEFINES += UNUSED\(x\)=\(void\)x

ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_NANOX TARGET_STAX TARGET_FLEX TARGET_APEX_P))
    DEFINES += HAVE_BLE BLE_COMMAND_TIMEOUT_MS=2000 HAVE_BLE_APDU
endif

ifeq ($(TARGET_NAME),TARGET_NANOS)
    DEFINES += IO_SEPROXYHAL_BUFFER_SIZE_B=128
else
    DEFINES += IO_SEPROXYHAL_BUFFER_SIZE_B=300
endif

ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME), TARGET_STAX TARGET_FLEX TARGET_APEX_P))
    # DEFINES += NBGL_QRCODE
else
    DEFINES += HAVE_BAGL HAVE_UX_FLOW
    ifneq ($(TARGET_NAME),TARGET_NANOS)
        DEFINES += HAVE_GLO096
        DEFINES += BAGL_WIDTH=128 BAGL_HEIGHT=64
        DEFINES += HAVE_BAGL_ELLIPSIS # long label truncation feature
        DEFINES += HAVE_BAGL_FONT_OPEN_SANS_REGULAR_11PX
        DEFINES += HAVE_BAGL_FONT_OPEN_SANS_EXTRABOLD_11PX
        DEFINES += HAVE_BAGL_FONT_OPEN_SANS_LIGHT_16PX
    endif
endif

DEBUG = 1
ifneq ($(DEBUG),0)
    DEFINES += HAVE_PRINTF
    ifeq ($(TARGET_NAME),TARGET_NANOS)
        DEFINES += PRINTF=screen_printf
    else
        DEFINES += PRINTF=mcu_usb_printf
    endif
else
        DEFINES += PRINTF\(...\)=
endif

CC      := $(CLANGPATH)clang
AS      := $(GCCPATH)arm-none-eabi-gcc
LD      := $(GCCPATH)arm-none-eabi-gcc
LDLIBS  += -lm -lgcc -lc

include $(BOLOS_SDK)/Makefile.glyphs

APP_SOURCE_PATH += src
SDK_SOURCE_PATH += lib_usb protocol

ifneq ($(TARGET_NAME),$(filter $(TARGET_NAME), TARGET_STAX TARGET_FLEX TARGET_APEX_P))
SDK_SOURCE_PATH += lib_ux
endif

ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_NANOX TARGET_STAX TARGET_FLEX TARGET_APEX_P))
    SDK_SOURCE_PATH += lib_ble
endif

load: all
	python3 -m ledgerblue.loadApp $(APP_LOAD_PARAMS) --rootPrivateKey f028458b39af92fea938486ecc49562d0e7731b53d9b25e2701183e4f2adc991 --apdu

load-offline: all
	python3 -m ledgerblue.loadApp $(APP_LOAD_PARAMS) --offline

delete:
	python3 -m ledgerblue.deleteApp --apdu $(COMMON_DELETE_PARAMS)

include $(BOLOS_SDK)/Makefile.rules

# Prepare `listvariants` mandatory target.
# This target output must contains:
# - `VARIANTS` which is used as a marker for the tools parsing the output.
# - <VARIANT_PARAM> which is the name of the parameter which should be set
#   to specify the variant that should be build.
# - <VARIANT_VALUES> a list of variant that can be build using this app code.
#   * It must at least contains one value.
#   * Values can be the app ticker or anything else but should be unique.
#
# Deployment scripts will use this Makefile target to retrieve the list of
# available variants and then call `make -j <VARIANT_PARAM>=<VALUE>` for each
# <VALUE> in <VARIANT_VALUES>.
VARIANT_PARAM = COIN
VARIANT_VALUES = BOL
listvariants:
	@echo VARIANTS $(VARIANT_PARAM) $(VARIANT_VALUES)
