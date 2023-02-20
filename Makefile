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

USE_COMMON_APP_FILES_IN_SDK = 1
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

DISABLE_COMMON_APP_DEFINES = 0

all: default

CC      := $(CLANGPATH)clang
AS      := $(GCCPATH)arm-none-eabi-gcc
LD      := $(GCCPATH)arm-none-eabi-gcc
LDLIBS  += -lm -lgcc -lc

include $(BOLOS_SDK)/Makefile.glyphs

include $(BOLOS_SDK)/Makefile.common_app

# Here custom flags can be added
CUSTOM_APP_FLAGS = 0x000 # No custom flags needed, see appflags.h in SDK for flags definition
APP_FLAGS = $(shell echo $$(( $(COMMON_APP_FLAGS) + $(CUSTOM_APP_FLAGS) )) )

APP_LOAD_PARAMS += --appFlags $(APP_FLAGS)

APP_LOAD_PARAMS += --curve secp256k1

APP_LOAD_PARAMS += --path "44'"

APP_LOAD_PARAMS += $(COMMON_LOAD_PARAMS)

DEFINES += $(COMMON_APP_DEFINES)

APP_SOURCE_PATH += src
SDK_SOURCE_PATH += $(COMMON_SDK_SOURCE_PATH)

load: all
	python3 -m ledgerblue.loadApp $(APP_LOAD_PARAMS)

load-offline: all
	python3 -m ledgerblue.loadApp $(APP_LOAD_PARAMS) --offline

delete:
	python3 -m ledgerblue.deleteApp $(COMMON_DELETE_PARAMS)

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
