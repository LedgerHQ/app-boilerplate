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

USE_STANDARD_APP_FILES_IN_SDK = 1

APPNAME      = "Boilerplate"
APPVERSION_M = 1
APPVERSION_N = 0
APPVERSION_P = 1
APPVERSION   = "$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)"

# - <VARIANT_PARAM> is the name of the parameter which should be set
#   to specify the variant that should be build.
# - <VARIANT_VALUES> a list of variant that can be build using this app code.
#   * It must at least contains one value.
#   * Values can be the app ticker or anything else but should be unique.
VARIANT_PARAM = COIN
VARIANT_VALUES = BOL

APP_LOAD_PARAMS += --curve secp256k1
APP_LOAD_PARAMS += --path "44'/1'"   # purpose=coin(44) / coin_type=Testnet(1)

ifeq ($(TARGET_NAME),TARGET_NANOS)
    ICONNAME=icons/nanos_app_boilerplate.gif
else ifeq ($(TARGET_NAME),TARGET_STAX)
    ICONNAME=icons/stax_app_boilerplate_32px.gif
else
    ICONNAME=icons/nanox_app_boilerplate.gif
endif

ifeq ($(TARGET_NAME), TARGET_STAX)
	ENABLE_NBGL_QRCODE   = 1
	ENABLE_NBGL_KEYBOARD = 0
	ENABLE_NBGL_KEYPAD   = 0
endif

ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_NANOX TARGET_STAX))
    ENABLE_BLUETOOTH = 1
endif

# Enabling DEBUG flag will enable PRINTF and disable optimizations
DEBUG ?= 0

# Default IO SEPROXY BUFFER SIZE must me explicitly disabled to define custom size
DISABLE_DEFAULT_IO_SEPROXY_BUFFER_SIZE = 0

DISABLE_STANDARD_APP_DEFINES = 0

APP_SOURCE_PATH += src

all: default


CC      := $(CLANGPATH)clang
AS      := $(GCCPATH)arm-none-eabi-gcc
LD      := $(GCCPATH)arm-none-eabi-gcc
LDLIBS  += -lm -lgcc -lc

include $(BOLOS_SDK)/Makefile.glyphs

include $(BOLOS_SDK)/Makefile.standard_app
