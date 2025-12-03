cmake_minimum_required(VERSION 3.10)

project(CardanoFuzzers
        VERSION 1.0
        DESCRIPTION "Fuzzing harnesses for Cardano Ledger app"
        LANGUAGES C)

set(CMAKE_C_STANDARD 11)

if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Fuzzer needs to be built with Clang")
endif()

# guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

if (NOT DEFINED ENV{LIB_FUZZING_ENGINE})
    add_compile_options(-fsanitize=address,fuzzer-no-link)
    add_link_options(-fsanitize=address,fuzzer)
else()
    add_link_options($ENV{LIB_FUZZING_ENGINE})
endif()

add_compile_options(-g)

set(SDK_PATH ${BOLOS_SDK})
set(CARDANO_PATH ..)

# Core Cardano app sources
set(CARDANO_SOURCE
    ${CARDANO_PATH}/src/addressUtils/addressUtilsByron.c
    ${CARDANO_PATH}/src/addressUtils/addressUtilsShelley.c
    ${CARDANO_PATH}/src/addressUtils/bech32.c
    ${CARDANO_PATH}/src/addressUtils/bip44.c
    ${CARDANO_PATH}/src/apdu/dispatcher.c
    ${CARDANO_PATH}/src/app_tokens/app_tokens.c
    ${CARDANO_PATH}/src/auxDataHashBuilder/auxDataHashBuilder.c
    ${CARDANO_PATH}/src/crypto/crypto.c
    ${CARDANO_PATH}/src/handler/get_app_name.c
    ${CARDANO_PATH}/src/handler/get_public_key.c
    ${CARDANO_PATH}/src/handler/get_serial.c
    ${CARDANO_PATH}/src/handler/get_version.c
    ${CARDANO_PATH}/src/handler/sign_opcert.c
    ${CARDANO_PATH}/src/handler/sign_tx.c
    ${CARDANO_PATH}/src/keyDerivation/keyDerivation.c
    ${CARDANO_PATH}/src/mem.c
    ${CARDANO_PATH}/src/mem_utils.c
    ${CARDANO_PATH}/src/messageSigning/messageSigning.c
    ${CARDANO_PATH}/src/opcert/parse_opcert.c
    ${CARDANO_PATH}/src/securityPolicy/securityPolicy.c
    ${CARDANO_PATH}/src/securityPolicy/securityPolicy_privkey.c
    ${CARDANO_PATH}/src/transaction/deserialize.c
    ${CARDANO_PATH}/src/transaction/tx_utils.c
    ${CARDANO_PATH}/src/transaction/tx_warnings.c
    ${CARDANO_PATH}/src/txHashBuilder/txHashBuilder.c
    ${CARDANO_PATH}/src/ui/menu_nbgl.c
    ${CARDANO_PATH}/src/ui/nbgl_display_opcert.c
    ${CARDANO_PATH}/src/ui/nbgl_display_pubkey.c
    ${CARDANO_PATH}/src/ui/nbgl_display_transaction.c
    ${CARDANO_PATH}/src/ui/nbgl_display_witness.c
    ${CARDANO_PATH}/src/ui/nbgl_screens.c
    ${CARDANO_PATH}/src/ui/ui_utils.c
    ${CARDANO_PATH}/src/utils/buffer_utils.c
    ${CARDANO_PATH}/src/utils/cbor.c
    ${CARDANO_PATH}/src/utils/hexUtils.c
    ${CARDANO_PATH}/src/utils/ipUtils.c
    ${CARDANO_PATH}/src/utils/list.c
    ${CARDANO_PATH}/src/utils/textUtils.c
    ${BOLOS_SDK}/lib_alloc/mem_alloc.c
    ${BOLOS_SDK}/lib_standard_app/base58.c
    ${BOLOS_SDK}/lib_standard_app/bip32.c
    ${BOLOS_SDK}/lib_standard_app/buffer.c
    ${BOLOS_SDK}/lib_standard_app/format.c
    ${BOLOS_SDK}/lib_standard_app/read.c
    ${BOLOS_SDK}/lib_standard_app/varint.c
    ${BOLOS_SDK}/lib_standard_app/write.c
)

include_directories(
    ${BOLOS_SDK}/include
    ${BOLOS_SDK}/target/stax/include
    ${BOLOS_SDK}/lib_alloc
    ${BOLOS_SDK}/lib_cxng/include
    ${BOLOS_SDK}/lib_nbgl/include
    ${BOLOS_SDK}/lib_ux/include
    ${BOLOS_SDK}/lib_ux_nbgl
    ${BOLOS_SDK}/lib_standard_app
    ${BOLOS_SDK}/io/include
    ${BOLOS_SDK}/io_legacy/include

    ${CARDANO_PATH}/src
    ${CARDANO_PATH}/src/addressUtils
    ${CARDANO_PATH}/src/apdu
    ${CARDANO_PATH}/src/app_tokens
    ${CARDANO_PATH}/src/auxDataHashBuilder
    ${CARDANO_PATH}/src/crypto
    ${CARDANO_PATH}/src/handler
    ${CARDANO_PATH}/src/keyDerivation
    ${CARDANO_PATH}/src/messageSigning
    ${CARDANO_PATH}/src/opcert
    ${CARDANO_PATH}/src/securityPolicy
    ${CARDANO_PATH}/src/transaction
    ${CARDANO_PATH}/src/txHashBuilder
    ${CARDANO_PATH}/src/ui
    ${CARDANO_PATH}/src/utils
    ./include
)

add_compile_definitions(
    FUZZING
    HAVE_NBGL
    NBGL_PAGE
    HAVE_PIEZO_SOUND
    TARGET_STAX
    HAVE_SE_TOUCH
    SCREEN_SIZE_WALLET

    APPNAME=\"Cardano\"
    MAJOR_VERSION=1
    MINOR_VERSION=1
    PATCH_VERSION=1
    APPVERSION=\"1.1.1\"

    IO_HID_EP_LENGTH=64
    IO_SEPROXYHAL_BUFFER_SIZE_B=300
    OS_IO_SEPROXYHAL

    HAVE_ECC
    HAVE_CRC
    HAVE_BLAKE2
    HAVE_ECC_WEIERSTRASS
    HAVE_SECP256K1_CURVE
    HAVE_SECP256R1_CURVE
    HAVE_ECC_TWISTED_EDWARDS
    HAVE_ED25519_CURVE
    HAVE_ECDSA
    HAVE_EDDSA
    HAVE_HASH
    HAVE_SHA224
    HAVE_SHA256
    HAVE_SHA3

    APP_FEATURE_OPCERT

    OS_IO_SEPH_BUFFER_SIZE=300
)

add_compile_options(
    -Wno-format -Wno-pointer-to-int-cast -Wno-constant-conversion -Wno-tautological-constant-out-of-range-compare
)

set(SOURCE
    ${CARDANO_SOURCE}
    ./src/os_mocks.c
    ./src/ui_mocks.c
    ./src/crc32.c
)

add_library(cardano_fuzz ${SOURCE})

set(harnesses
    fuzz_signOpCert
    fuzz_getPublicKeys
    fuzz_all_handlers
)

foreach(harness IN LISTS harnesses)
    add_executable(${harness}
        ./src/${harness}.c
    )
    target_link_libraries(${harness} PUBLIC cardano_fuzz)
endforeach()
