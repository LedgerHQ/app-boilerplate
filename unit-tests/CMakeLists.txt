cmake_minimum_required(VERSION 3.10)

if(${CMAKE_VERSION} VERSION_LESS 3.10)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# project information
project(unit_tests
        VERSION 0.1
	      DESCRIPTION "Unit tests for Ledger Nano application"
        LANGUAGES C)


# guard against bad build-type strings
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

include(CTest)
ENABLE_TESTING()

# specify C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -pedantic -g -O0 --coverage")

set(GCC_COVERAGE_LINK_FLAGS "--coverage -lgcov")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")

# guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

add_compile_definitions(TEST)
add_compile_definitions(UNIT_TEST)
add_compile_definitions(DEBUG)
add_compile_definitions(FUZZING)

# IMPORTANT: mock_includes MUST come first so our SDK mocks override Ledger SDK headers
# (We use real headers from ../src for app-specific code like utils.h, assert.h, etc.)
include_directories(mock_includes)
include_directories(libs)
include_directories(../src)
include_directories(../src/utils)
include_directories(../src/memory)
include_directories(../src/addressUtils)
include_directories(../src/crypto)
include_directories(../src/transaction)
include_directories(../src/opcert)
include_directories(../src/securityPolicy)
include_directories(../src/messageSigning)
include_directories(../src/apdu)
include_directories(../src/txHashBuilder)
include_directories($ENV{BOLOS_SDK}/lib_standard_app)
include_directories($ENV{BOLOS_SDK}/include)
include_directories($ENV{BOLOS_SDK}/lib_alloc)
include_directories($ENV{BOLOS_SDK}/lib_cxng/include)
include_directories($ENV{BOLOS_SDK}/lib_nbgl/include)

add_compile_definitions(HAVE_CRC)
add_compile_definitions(HAVE_PRINTF)
# Prevent os.h from defining PRINTF as empty macro
add_compile_definitions(PRINTF=PRINTF)
add_compile_definitions(APPNAME=\"Cardano\")
add_compile_definitions(APPVERSION=\"8.0.0\")
add_compile_definitions(MAJOR_VERSION=8)
add_compile_definitions(MINOR_VERSION=0)
add_compile_definitions(PATCH_VERSION=0)

# Test executables
add_executable(test_hex_utils test_hex_utils.c)
add_executable(test_base58 test_base58.c)
add_executable(test_bech32 test_bech32.c)
add_executable(test_text_utils test_text_utils.c)
add_executable(test_bip44 test_bip44.c)
add_executable(test_cbor test_cbor.c)
add_executable(test_buffer_utils test_buffer_utils.c)
add_executable(test_ip_utils test_ip_utils.c ../src/utils/ipUtils.c)
add_executable(test_hash test_hash.c)
add_executable(test_apdu_info
    test_apdu_info.c
    ../src/handler/get_version.c
    ../src/handler/get_app_name.c
    ../src/handler/get_serial.c
    mock_sources/io_stub.c
)
add_executable(test_address_utils_byron
    test_address_utils_byron.c
    ../src/addressUtils/addressUtilsByron.c
    ../src/keyDerivation/keyDerivation.c
    ../src/securityPolicy/securityPolicy_privkey.c
)
add_executable(test_address_utils_shelley
    test_address_utils_shelley.c
    ../src/addressUtils/addressUtilsShelley.c
    ../src/addressUtils/addressUtilsByron.c
    ../src/keyDerivation/keyDerivation.c
    ../src/securityPolicy/securityPolicy_privkey.c
)
add_executable(test_key_derivation
    test_key_derivation.c
    ../src/keyDerivation/keyDerivation.c
    ../src/securityPolicy/securityPolicy_privkey.c
)
add_executable(test_message_signing
    test_message_signing.c
    ../src/messageSigning/messageSigning.c
    ../src/keyDerivation/keyDerivation.c
    ../src/securityPolicy/securityPolicy_privkey.c
)
add_executable(test_aux_data_hash_builder
    test_aux_data_hash_builder.c
    ../src/auxDataHashBuilder/auxDataHashBuilder.c
)
add_executable(test_tx_hash_builder
    test_tx_hash_builder.c
    ../src/transaction/tx_hash_builder.c
)
add_executable(test_tx_prepare
    test_tx_prepare.c
    mock_globals.c
    ../src/transaction/tx_validate.c
    ../src/transaction/tx_parse.c
    ../src/transaction/tx_parse_certificates.c
    ../src/transaction/tx_parse_outputs.c
    ../src/transaction/tx_utils.c
    ../src/transaction/tx_hash_builder.c
    ../src/addressUtils/addressUtilsShelley.c
    ../src/addressUtils/addressUtilsByron.c
    ../src/addressUtils/bip44.c
    ../src/securityPolicy/securityPolicy.c
    ../src/securityPolicy/securityPolicy_privkey.c
    ../src/keyDerivation/keyDerivation.c
    ../src/memory/mem.c
    ../src/memory/mem_utils.c
    ../src/memory/flist.c
    ../src/app_tokens/app_tokens.c
    ../src/ui/ui_utils.c
    ../src/ui/ui_warnings.c
    ../src/ui/tx_ui_format.c
    ../src/ui/tx_ui_helpers.c
    ../src/app_context.c
    mock_sources/mem_alloc_stub.c
    mock_sources/glyphs_mock.c
    mock_sources/display_stub.c
    mock_sources/hex_stub.c
    mock_sources/io_stub.c
)
add_executable(test_ui_utils
    test_ui_utils.c
    ../src/ui/ui_utils.c
    ../src/memory/mem.c
    ../src/memory/mem_utils.c
    mock_sources/mem_alloc_stub.c
)

set(TEST_EXECUTABLES
    test_hex_utils
    test_base58
    test_bech32
    test_text_utils
    test_bip44
    test_cbor
    test_buffer_utils
    test_ip_utils
    test_hash
    test_apdu_info
    test_address_utils_byron
    test_address_utils_shelley
    test_key_derivation
    test_message_signing
    test_aux_data_hash_builder
    test_tx_hash_builder
    test_tx_prepare
    test_ui_utils
)
# add_executable(test_tokens test_tokens.c)  # TODO: needs crypto mocks (keyDerivation.h, etc.)
# add_executable(test_tx_parser test_tx_parser.c)  # TODO: create this file
# add_executable(test_tx_utils test_tx_utils.c)    # TODO: create this file

# SDK libraries
add_library(base58 SHARED $ENV{BOLOS_SDK}/lib_standard_app/base58.c)
add_library(bip32 SHARED $ENV{BOLOS_SDK}/lib_standard_app/bip32.c)
add_library(buffer SHARED $ENV{BOLOS_SDK}/lib_standard_app/buffer.c)
target_link_libraries(buffer PUBLIC varint bip32)
add_library(read SHARED $ENV{BOLOS_SDK}/lib_standard_app/read.c)
add_library(write SHARED $ENV{BOLOS_SDK}/lib_standard_app/write.c)
add_library(format SHARED $ENV{BOLOS_SDK}/lib_standard_app/format.c)
add_library(varint SHARED $ENV{BOLOS_SDK}/lib_standard_app/varint.c)
target_link_libraries(varint PUBLIC write)
add_library(apdu_parser SHARED $ENV{BOLOS_SDK}/lib_standard_app/parser.c)

# App-specific libraries (testing the REAL source files!)
# The mock_includes/ directory is prioritized so our SDK mocks override Ledger SDK headers
add_library(hex_utils SHARED hexUtils.c)
target_link_libraries(hex_utils PUBLIC c)
add_library(bech32_lib SHARED ../src/addressUtils/bech32.c)
target_link_libraries(bech32_lib PUBLIC c)
add_library(text_utils SHARED ../src/utils/textUtils.c ../src/utils/ipUtils.c)
target_link_libraries(text_utils PUBLIC c format)
add_library(bip44_lib SHARED libs/format_bip44_path.c)  # Standalone version without crypto dependencies
target_link_libraries(bip44_lib PUBLIC c)
add_library(bip44_real SHARED ../src/addressUtils/bip44.c)
target_link_libraries(bip44_real PUBLIC c read write buffer varint bip32 hash_mocks_lib)
add_library(cbor_lib SHARED ../src/utils/cbor.c)
target_link_libraries(cbor_lib PUBLIC c m read)
add_library(buffer_utils_lib SHARED ../src/utils/buffer_utils.c)
target_link_libraries(buffer_utils_lib PUBLIC c cbor_lib buffer write)
add_library(crypto_mocks_lib SHARED
    mocks/crypto_mocks.c
)
target_link_libraries(crypto_mocks_lib PUBLIC c)
add_library(mock_printf STATIC mock_sources/printf_stub.c)
target_link_libraries(mock_printf PUBLIC c)
add_library(cx_crc_support SHARED
    $ENV{BOLOS_SDK}/lib_cxng/src/cx_crc16.c
    $ENV{BOLOS_SDK}/lib_cxng/src/cx_crc32.c
    $ENV{BOLOS_SDK}/unit-tests/mock/src/cx_crc.c
)
target_link_libraries(cx_crc_support PUBLIC c)

set(SIGN_TX_COMMON_SOURCES
    mock_globals.c
    ../src/memory/flist.c
    ../src/handler/sign_tx.c
    ../src/transaction/tx_parse.c
    ../src/transaction/tx_parse_certificates.c
    ../src/transaction/tx_parse_outputs.c
    ../src/transaction/tx_utils.c
    ../src/securityPolicy/securityPolicy.c
    ../src/securityPolicy/securityPolicy_privkey.c
    ../src/messageSigning/messageSigning.c
    ../src/keyDerivation/keyDerivation.c
    ../src/app_tokens/app_tokens.c
    ../src/addressUtils/addressUtilsShelley.c
    ../src/addressUtils/addressUtilsByron.c
    ../src/transaction/tx_hash_builder.c
    ../src/transaction/tx_validate.c
    ../src/ui/tx_ui_format.c
    ../src/ui/tx_ui_helpers.c
    ../src/ui/ui_utils.c
    ../src/ui/ui_warnings.c
    ../src/app_context.c
    ../src/memory/mem_utils.c
    mock_sources/mem_alloc_stub.c
    mock_sources/glyphs_mock.c
    mock_sources/display_stub.c
    mock_sources/hex_stub.c
    mock_sources/io_stub.c
)
set(SIGN_TX_COMMON_LIBS
    text_utils
    hex_utils
    base58
    bech32_lib
    bip44_real
    buffer_utils_lib
    cbor_lib
    blake2b_lib
    sha3_lib
    hash_mocks_lib
    crypto_mocks_lib
    buffer
    write
    read
    varint
    bip32
    cx_crc_support
    mock_printf
)
add_library(cardano_sign_tx_core STATIC ${SIGN_TX_COMMON_SOURCES})
target_link_libraries(cardano_sign_tx_core PUBLIC ${SIGN_TX_COMMON_LIBS})
target_compile_definitions(cardano_sign_tx_core PRIVATE TRACE_TX_HASH_BUILDER)

# Crypto reference implementations
add_library(blake2b_lib SHARED libs/blake2b.c)
target_link_libraries(blake2b_lib PUBLIC c)
add_library(sha3_lib SHARED libs/sha3-256.c)
target_link_libraries(sha3_lib PUBLIC c)
add_library(hash_mocks_lib SHARED libs/hash_mocks.c)
target_link_libraries(hash_mocks_lib PUBLIC c)

# Link hash_mocks to crypto libs
target_link_libraries(hash_mocks_lib PUBLIC blake2b_lib sha3_lib)


# Link test executables
target_link_libraries(test_hex_utils PUBLIC cmocka gcov hex_utils)
target_link_libraries(test_base58 PUBLIC cmocka gcov base58 hex_utils)
target_link_libraries(test_bech32 PUBLIC cmocka gcov bech32_lib hex_utils)
target_link_libraries(test_text_utils PUBLIC cmocka gcov text_utils)
target_link_libraries(test_bip44 PUBLIC cmocka gcov bip44_lib)
target_link_libraries(test_cbor PUBLIC cmocka gcov cbor_lib hex_utils read write c)
target_link_libraries(test_buffer_utils PUBLIC cmocka gcov buffer_utils_lib buffer write varint bip32)
target_link_libraries(test_ip_utils PUBLIC cmocka gcov)
target_link_libraries(test_hash PUBLIC cmocka gcov hex_utils blake2b_lib sha3_lib hash_mocks_lib)
target_link_libraries(test_apdu_info PUBLIC cmocka gcov)
target_link_libraries(test_address_utils_byron
    PUBLIC
        cmocka
        gcov
        crypto_mocks_lib
        bip44_real
        hex_utils
        hash_mocks_lib
        blake2b_lib
        sha3_lib
        buffer_utils_lib
        cbor_lib
        buffer
        write
        read
        varint
        bip32
        cx_crc_support
)
target_link_libraries(test_address_utils_shelley
    PUBLIC
        cmocka
        gcov
        crypto_mocks_lib
        base58
        bech32_lib
        bip44_real
        hex_utils
        hash_mocks_lib
        blake2b_lib
        sha3_lib
        buffer_utils_lib
        cbor_lib
        buffer
        write
        read
        varint
        bip32
        cx_crc_support
)
target_link_libraries(test_key_derivation PUBLIC cmocka gcov crypto_mocks_lib bip44_real hex_utils write)
target_link_libraries(test_message_signing PUBLIC cmocka gcov crypto_mocks_lib bip44_real)
target_link_libraries(test_aux_data_hash_builder
    PUBLIC
        cmocka
        gcov
        hex_utils
        cbor_lib
        hash_mocks_lib
        blake2b_lib
        sha3_lib
        read
        write
)
add_test(test_aux_data_hash_builder test_aux_data_hash_builder)
target_link_libraries(test_tx_hash_builder
    PUBLIC
        cmocka
        gcov
        hash_mocks_lib
        blake2b_lib
        sha3_lib
        hex_utils
        cbor_lib
        buffer_utils_lib
        buffer
        write
        read
        varint
        bip32
)
target_link_libraries(test_tx_prepare
    PUBLIC
        cmocka
        gcov
        hex_utils
        text_utils
        blake2b_lib
        sha3_lib
        hash_mocks_lib
        cbor_lib
        base58
        bech32_lib
        bip44_real
        buffer_utils_lib
        buffer
        write
        read
        varint
        bip32
        format
        crypto_mocks_lib
        cx_crc_support
)
add_test(test_tx_hash_builder test_tx_hash_builder)

# Mary era tests (multiasset/tokens)
add_executable(test_sign_tx_mary test_sign_tx_mary.c)
target_link_libraries(test_sign_tx_mary PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_mary test_sign_tx_mary)

add_executable(test_sign_tx_shelley test_sign_tx_shelley.c)
target_link_libraries(test_sign_tx_shelley PUBLIC cmocka gcov cardano_sign_tx_core)
target_compile_definitions(test_sign_tx_shelley PRIVATE TRACE_TX_HASH_BUILDER)
add_test(test_sign_tx_shelley test_sign_tx_shelley)

add_executable(test_sign_tx_byron test_sign_tx_byron.c)
target_link_libraries(test_sign_tx_byron PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_byron test_sign_tx_byron)

# Shelley era tests with certificates
add_executable(test_sign_tx_shelley_certificates test_sign_tx_shelley_certificates.c)
target_link_libraries(test_sign_tx_shelley_certificates PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_shelley_certificates test_sign_tx_shelley_certificates)

# Allegra era tests (validity intervals)
add_executable(test_sign_tx_allegra test_sign_tx_allegra.c)
target_link_libraries(test_sign_tx_allegra PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_allegra test_sign_tx_allegra)

# Alonzo era tests (Plutus scripts)
add_executable(test_sign_tx_alonzo test_sign_tx_alonzo.c)
target_link_libraries(test_sign_tx_alonzo PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_alonzo test_sign_tx_alonzo)
add_executable(test_sign_tx_alonzo_catalyst test_sign_tx_alonzo_catalyst.c)
target_link_libraries(test_sign_tx_alonzo_catalyst PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_alonzo_catalyst test_sign_tx_alonzo_catalyst)
add_executable(test_sign_tx_alonzo_cip36 test_sign_tx_alonzo_cip36.c)
target_link_libraries(test_sign_tx_alonzo_cip36 PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_alonzo_cip36 test_sign_tx_alonzo_cip36)

# Babbage era tests (reference inputs, inline datums)
add_executable(test_sign_tx_babbage test_sign_tx_babbage.c)
target_link_libraries(test_sign_tx_babbage PUBLIC cmocka gcov cardano_sign_tx_core)
target_compile_definitions(test_sign_tx_babbage PRIVATE TRACE_TX_HASH_BUILDER)
add_test(test_sign_tx_babbage test_sign_tx_babbage)

# Conway era tests with governance certificates
add_executable(test_sign_tx_conway test_sign_tx_conway.c)
target_link_libraries(test_sign_tx_conway PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_conway test_sign_tx_conway)

# Conway era tests without certificates (treasury/donation)
add_executable(test_sign_tx_conway_without_certificates test_sign_tx_conway_without_certificates.c)
target_link_libraries(test_sign_tx_conway_without_certificates PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_conway_without_certificates test_sign_tx_conway_without_certificates)

# Conway era voting procedures tests
add_executable(test_sign_tx_conway_voting test_sign_tx_conway_voting.c)
target_link_libraries(test_sign_tx_conway_voting PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_conway_voting test_sign_tx_conway_voting)

# Multisig tests
add_executable(test_sign_tx_multisig test_sign_tx_multisig.c)
target_link_libraries(test_sign_tx_multisig PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_multisig test_sign_tx_multisig)

# Pool registration tests
add_executable(test_sign_tx_pool_registration test_sign_tx_pool_registration.c)
target_link_libraries(test_sign_tx_pool_registration PUBLIC cmocka gcov cardano_sign_tx_core)
add_test(test_sign_tx_pool_registration test_sign_tx_pool_registration)

foreach(target IN LISTS TEST_EXECUTABLES)
    target_link_libraries(${target} PUBLIC mock_printf)
endforeach()

add_executable(test_sign_tx_rejects
    test_sign_tx_rejects.c
    # mem implementation is provided by test fixtures (test_sign_tx_rejects.c) to avoid
    # duplicate allocator state in mocks.
    mock_sources/mem_alloc_stub.c
)
target_link_libraries(test_sign_tx_rejects PUBLIC cmocka gcov cardano_sign_tx_core)
target_link_libraries(test_ui_utils PUBLIC cmocka gcov)
add_test(test_sign_tx_rejects test_sign_tx_rejects)
# target_link_libraries(test_tokens PUBLIC cmocka gcov tokens_lib bech32_lib hex_utils)  # TODO: when tokens_lib is ready

# TODO: uncomment when test files are created
# target_link_libraries(test_tx_parser PUBLIC
#                       transaction_deserialize
#                       transaction_serialize
#                       buffer
#                       bip32
#                       cmocka
#                       gcov
#                       write
#                       read
#                       varint
#                       transaction_utils)
# target_link_libraries(test_tx_utils PUBLIC
#                       cmocka
#                       gcov
#                       transaction_utils)

# Register tests with CTest
add_test(test_hex_utils test_hex_utils)
add_test(test_apdu_info test_apdu_info)
add_test(test_base58 test_base58)
add_test(test_bech32 test_bech32)
add_test(test_text_utils test_text_utils)
add_test(test_bip44 test_bip44)
add_test(test_cbor test_cbor)
add_test(test_buffer_utils test_buffer_utils)
add_test(test_ip_utils test_ip_utils)
add_test(test_hash test_hash)
add_test(test_address_utils_byron test_address_utils_byron)
add_test(test_address_utils_shelley test_address_utils_shelley)
add_test(test_key_derivation test_key_derivation)
add_test(test_message_signing test_message_signing)

add_executable(test_opcert_message test_opcert_message.c hexUtils.c)
target_link_libraries(test_opcert_message PUBLIC cmocka gcov)
add_test(test_opcert_message test_opcert_message)

# add_test(test_tokens test_tokens)  # TODO: uncomment when crypto dependencies are mocked
# add_test(test_tx_parser test_tx_parser)  # TODO: uncomment when created
# add_test(test_tx_utils test_tx_utils)    # TODO: uncomment when created
