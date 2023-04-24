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

APPNAME      = "Boilerplate"
APPVERSION_M = 1
APPVERSION_N = 0
APPVERSION_P = 1

# - <VARIANT_VALUES> a list of variant that can be build using this app code.
#   * It must at least contains one value.
#   * Values can be the app ticker or anything else but should be unique.
VARIANT_VALUES = BOL

CURVE_APP_LOAD_PARAMS = secp256k1
PATH_APP_LOAD_PARAMS = "44'/1'"   # purpose=coin(44) / coin_type=Testnet(1)

ICON_NANOS = icons/nanos_app_boilerplate.gif
ICON_NANOX = icons/nanox_app_boilerplate.gif
ICON_NANOSP = icons/nanox_app_boilerplate.gif
ICON_STAX = icons/stax_app_boilerplate_32px.gif

ENABLE_BLUETOOTH = 1

# Enabling DEBUG flag will enable PRINTF and disable optimizations
DEBUG ?= 0

ENABLE_NBGL_QRCODE   = 1

APP_SOURCE_PATH += src

include $(BOLOS_SDK)/Makefile.standard_app
