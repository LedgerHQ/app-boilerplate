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
add_compile_definitions(DEVEL)
add_compile_definitions(FUZZING)

# IMPORTANT: mock_includes MUST come first so our SDK mocks override Ledger SDK headers
# (We use real headers from ../src for app-specific code like utils.h, assert.h, etc.)
include_directories(mock_includes)
include_directories(libs)
include_directories(../src)
include_directories(../src/utils)
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
include_directories($ENV{BOLOS_SDK}/lib_cxng/include)
include_directories($ENV{BOLOS_SDK}/lib_nbgl/include)

add_compile_definitions(HAVE_CRC)

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
    ../src/txHashBuilder/txHashBuilder.c
)
add_executable(test_sign_tx_handler
    test_sign_tx_handler.c
    mock_globals.c
    ../src/utils/list.c
    ../src/handler/sign_tx.c
    ../src/transaction/deserialize.c
    ../src/transaction/tx_utils.c
    ../src/transaction/tx_warnings.c
    ../src/securityPolicy/securityPolicy.c
    ../src/securityPolicy/securityPolicy_privkey.c
    ../src/messageSigning/messageSigning.c
    ../src/keyDerivation/keyDerivation.c
    ../src/addressUtils/addressUtilsShelley.c
    ../src/addressUtils/addressUtilsByron.c
    ../src/txHashBuilder/txHashBuilder.c
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
add_library(hex_utils SHARED ../src/utils/hexUtils.c)
target_link_libraries(hex_utils PUBLIC c)
add_library(bech32_lib SHARED ../src/addressUtils/bech32.c)
target_link_libraries(bech32_lib PUBLIC c)
add_library(text_utils SHARED ../src/utils/textUtils.c)
target_link_libraries(text_utils PUBLIC c)
add_library(bip44_lib SHARED libs/bip44_printToStr.c)  # Standalone version without crypto dependencies
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
add_library(cx_crc_support SHARED
    $ENV{BOLOS_SDK}/lib_cxng/src/cx_crc16.c
    $ENV{BOLOS_SDK}/lib_cxng/src/cx_crc32.c
    $ENV{BOLOS_SDK}/unit-tests/mock/src/cx_crc.c
)
target_link_libraries(cx_crc_support PUBLIC c)

# Crypto reference implementations
add_library(blake2b_lib SHARED libs/blake2b.c)
target_link_libraries(blake2b_lib PUBLIC c)
add_library(sha3_lib SHARED libs/sha3-256.c)
target_link_libraries(sha3_lib PUBLIC c)
add_library(hash_mocks_lib SHARED libs/hash_mocks.c)
target_link_libraries(hash_mocks_lib PUBLIC c)

# Link hash_mocks to crypto libs
target_link_libraries(hash_mocks_lib PUBLIC blake2b_lib sha3_lib)

# add_library(tokens_lib SHARED ../src/tokens/tokens.c)  # TODO: needs keyDerivation.h and crypto mocks
# add_library(transaction_deserialize SHARED ../src/transaction/deserialize.c)  # TODO: uncomment when needed
# add_library(transaction_serialize SHARED ../src/transaction/serialize.c)      # TODO: uncomment when needed
# add_library(transaction_utils SHARED ../src/transaction/utils.c)              # TODO: uncomment when needed

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
add_test(test_tx_hash_builder test_tx_hash_builder)
target_link_libraries(test_sign_tx_handler
    PUBLIC
        cmocka
        gcov
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
)
add_test(test_sign_tx_handler test_sign_tx_handler)

add_executable(test_sign_tx_rejects
    test_sign_tx_rejects.c
    mock_globals.c
    ../src/utils/list.c
    ../src/handler/sign_tx.c
    ../src/transaction/deserialize.c
    ../src/transaction/tx_utils.c
    ../src/transaction/tx_warnings.c
    ../src/securityPolicy/securityPolicy.c
    ../src/securityPolicy/securityPolicy_privkey.c
    ../src/messageSigning/messageSigning.c
    ../src/keyDerivation/keyDerivation.c
    ../src/addressUtils/addressUtilsShelley.c
    ../src/addressUtils/addressUtilsByron.c
    ../src/txHashBuilder/txHashBuilder.c
)
target_link_libraries(test_sign_tx_rejects
    PUBLIC
        cmocka
        gcov
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
)
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
# add_test(test_tokens test_tokens)  # TODO: uncomment when crypto dependencies are mocked
# add_test(test_tx_parser test_tx_parser)  # TODO: uncomment when created
# add_test(test_tx_utils test_tx_utils)    # TODO: uncomment when created
