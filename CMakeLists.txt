# ****************************************************************************
#    Ledger App Boilerplate
#    (c) 2026 Ledger SAS.
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

# The minimum CMake version is set by the SDK presets (CMakePresets.json)
cmake_minimum_required(VERSION ${LEDGER_CMAKE_MINIMUM_VERSION})

# Application version
project(app-boilerplate VERSION 2.4.0 LANGUAGES C)

include($ENV{BOLOS_SDK}/cmake/LedgerApp.cmake)

ledger_app(
    ########################################
    #        Mandatory configuration       #
    ########################################
    # Application name
    NAME        "Boilerplate"

    # Application copyright, displayed in the settings
    COPYRIGHT   "(c) 2026 Ledger SAS"

    # Application variants, selected with -DCOIN=<variant>
    VARIANT_PARAM COIN
    VARIANTS      BOL

    # Application source files: every `src/**/*.c`, minus EXCLUDE_SOURCES.
    # Every `src/` directory holding a header is an include directory.
    #EXCLUDE_SOURCES src/unused.c

    # Application icons following guidelines:
    # https://developers.ledger.com/docs/embedded-app/design-requirements/#device-icon
    ICON_NANOX  icons/app_boilerplate_14px.gif
    ICON_NANOS2 icons/app_boilerplate_14px.gif
    ICON_STAX   icons/app_boilerplate_32px.gif
    ICON_FLEX   icons/app_boilerplate_40px.gif
    ICON_APEX_P icons/app_boilerplate_32px_apex.png

    # With the Nano NBGL Design, the Home Screen icon is the reverse of the App icon:
    # It should be on white background, with rounded corners.
    # This definition allows the SDK to automatically generate it based on the App icon.
    # Please note that the icon is generated in the build directory, from its file name.
    ICON_HOME_NANO glyphs/home_boilerplate_14px.gif

    # Application glyphs
    GLYPHS_DIR  glyphs

    # Application allowed derivation curves.
    # Possibles curves are: secp256k1, secp256r1, ed25519 and bls12381g1
    # If your app needs it, you can specify multiple curves by using:
    # `CURVES <curve1> <curve2>`
    CURVES      secp256k1

    # Application allowed derivation paths.
    # You should request a specific path for your app.
    # This serve as an isolation mechanism.
    # Most application will have to request a path according to the BIP-0044
    # and SLIP-0044 standards.
    # If your app needs it, you can specify multiple path by using:
    # `PATHS "44'/1'" "45'/1'"`
    PATHS       "44'/1'"   # purpose=coin(44) / coin_type=Testnet(1)

    ########################################
    #     Application custom permissions   #
    ########################################
    # See SDK `include/appflags.h` for the purpose of each permission
    #PERMISSIONS
        #DERIVE_MASTER
        #GLOBAL_PIN
        #BOLOS_SETTINGS
        #LIBRARY

    FEATURES
        ########################################
        # Application communication interfaces #
        ########################################
        BLUETOOTH
        #NFC

        ########################################
        #         NBGL custom features         #
        ########################################
        NBGL_QRCODE
        #NBGL_KEYBOARD
        #NBGL_KEYPAD

        ########################################
        #       SWAP FEATURE FLAG              #
        # This flag enables the swap feature   #
        # in the Boilerplate application.      #
        ########################################
        # The full documentation is available at https://ledgerhq.github.io/app-exchange/
        # Two flags exist for enabling the SWAP
        #   - SWAP           will lead to the enabling of the swap related C code of the standard_app
        #                    AND will lead to the enabling of the APP_LOAD_PARAM required for os_lib_call working on device
        #   - TESTING_SWAP:  will lead to the enabling of the swap related C code of the standard_app
        #                    ONLY works on Speculos, not on device
        # Testing only SWAP flag
        TESTING_SWAP
        # Production enabled SWAP flag
        #SWAP

        ########################################
        #          TLV & PKI features          #
        ########################################
        # Both are used in the Dynamic Token example
        TLV_LIBRARY
        PKI_LIBRARY

    ########################################
    #          Features disablers          #
    ########################################
    # These advanced settings allow to disable some feature that are by
    # default enabled in the SDK, such as the stack protection and LTO.
    #DISABLE
        #STACK_PROTECTOR
        #LINK_TIME_OPTIMIZATION
        #STANDARD_APP_FILES
        #STANDARD_SNPRINTF
        #STANDARD_USB
        #STANDARD_WEBUSB
        #STANDARD_U2F
)

########################################
#              Unit tests              #
########################################
if(BUILD_UNIT_TESTS)
    target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit-tests/fakes)
    add_subdirectory(unit-tests)
endif()
