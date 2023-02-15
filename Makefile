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

include $(BOLOS_SDK)/Makefile.defines

APP_LOAD_PARAMS  = --curve secp256k1
ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_NANOX TARGET_STAX))
APP_LOAD_PARAMS += --appFlags 0x200  # APPLICATION_FLAG_BOLOS_SETTINGS
else
APP_LOAD_PARAMS += --appFlags 0x000
endif
APP_LOAD_PARAMS += --path "44'"
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
else
    ICONNAME=icons/nanox_app_boilerplate.gif
endif

all: default

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

ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_NANOX TARGET_STAX))
    DEFINES += HAVE_BLE BLE_COMMAND_TIMEOUT_MS=2000 HAVE_BLE_APDU
endif

ifeq ($(TARGET_NAME),TARGET_NANOS)
    DEFINES += IO_SEPROXYHAL_BUFFER_SIZE_B=128
else
    DEFINES += IO_SEPROXYHAL_BUFFER_SIZE_B=300
endif

ifeq ($(TARGET_NAME),TARGET_STAX)
    DEFINES += NBGL_QRCODE
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

DEBUG = 0
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
SDK_SOURCE_PATH += lib_stusb lib_stusb_impl

ifneq ($(TARGET_NAME),TARGET_STAX)
SDK_SOURCE_PATH += lib_ux
endif

ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_NANOX TARGET_STAX))
    SDK_SOURCE_PATH += lib_blewbxx lib_blewbxx_impl
endif

load: all
	python3 -m ledgerblue.loadApp $(APP_LOAD_PARAMS)

load-offline: all
	python3 -m ledgerblue.loadApp $(APP_LOAD_PARAMS) --offline

delete:
	python3 -m ledgerblue.deleteApp $(COMMON_DELETE_PARAMS)

include $(BOLOS_SDK)/Makefile.rules

dep/%.d: %.c Makefile

listvariants:
	@echo VARIANTS COIN BOL
