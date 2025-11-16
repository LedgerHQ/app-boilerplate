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

# IMPORTANT: mock_includes MUST come first so our SDK mocks override Ledger SDK headers
# (We use real headers from ../src for app-specific code like utils.h, assert.h, etc.)
include_directories(mock_includes)
include_directories(../src)
include_directories(../src/transaction)
include_directories($ENV{BOLOS_SDK}/lib_standard_app)

# Test executables
add_executable(test_hex_utils test_hex_utils.c)
add_executable(test_base58 test_base58.c)
add_executable(test_bech32 test_bech32.c)
# add_executable(test_tx_parser test_tx_parser.c)  # TODO: create this file
# add_executable(test_tx_utils test_tx_utils.c)    # TODO: create this file

# SDK libraries
add_library(base58 SHARED $ENV{BOLOS_SDK}/lib_standard_app/base58.c)
add_library(bip32 SHARED $ENV{BOLOS_SDK}/lib_standard_app/bip32.c)
add_library(buffer SHARED $ENV{BOLOS_SDK}/lib_standard_app/buffer.c)
add_library(read SHARED $ENV{BOLOS_SDK}/lib_standard_app/read.c)
add_library(write SHARED $ENV{BOLOS_SDK}/lib_standard_app/write.c)
add_library(format SHARED $ENV{BOLOS_SDK}/lib_standard_app/format.c)
add_library(varint SHARED $ENV{BOLOS_SDK}/lib_standard_app/varint.c)
add_library(apdu_parser SHARED $ENV{BOLOS_SDK}/lib_standard_app/parser.c)

# App-specific libraries (testing the REAL source files!)
# The mock_includes/ directory is prioritized, so our mock utils.h will be used
add_library(hex_utils SHARED ../src/utils/hexUtils.c)
add_library(bech32_lib SHARED ../src/addressUtils/bech32.c)
# add_library(transaction_deserialize SHARED ../src/transaction/deserialize.c)  # TODO: uncomment when needed
# add_library(transaction_serialize SHARED ../src/transaction/serialize.c)      # TODO: uncomment when needed
# add_library(transaction_utils SHARED ../src/transaction/utils.c)              # TODO: uncomment when needed

# Link test executables
target_link_libraries(test_hex_utils PUBLIC cmocka gcov hex_utils)
target_link_libraries(test_base58 PUBLIC cmocka gcov base58 hex_utils)
target_link_libraries(test_bech32 PUBLIC cmocka gcov bech32_lib hex_utils)

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
# add_test(test_tx_parser test_tx_parser)  # TODO: uncomment when created
# add_test(test_tx_utils test_tx_utils)    # TODO: uncomment when created
