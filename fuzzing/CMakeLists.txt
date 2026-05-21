include_guard()
cmake_minimum_required(VERSION 3.14)

if(${CMAKE_VERSION} VERSION_LESS 3.14)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(
  BoilerPlateFuzzer
  VERSION 1.0
  DESCRIPTION "Ledger App Boilerplate Fuzzer"
  LANGUAGES C)

if(NOT DEFINED BOLOS_SDK)
  message(FATAL_ERROR "BOLOS_SDK must be defined, CMake will exit.")
  return()
endif()

include(${BOLOS_SDK}/fuzzing/cmake/LedgerAppFuzz.cmake)
ledger_fuzz_setup()

set(APP_SOURCE_DIR ${CMAKE_SOURCE_DIR}/..)
file(GLOB_RECURSE C_SOURCES
  "${APP_SOURCE_DIR}/src/*.c"
  "${CMAKE_SOURCE_DIR}/mock/*.c"
)
list(REMOVE_ITEM C_SOURCES "${APP_SOURCE_DIR}/src/app_main.c")

# Legacy parser fuzzer kept for backwards compatibility with the pre-Absolution
# fuzzing entry point.
include(extra/TxParser.cmake)

target_link_libraries(txparser PUBLIC secure_sdk)

add_executable(fuzz_tx_parser fuzz_tx_parser.c)
target_link_libraries(fuzz_tx_parser PUBLIC txparser)

ledger_fuzz_add_app_target(
    SOURCES
        ${C_SOURCES}
    INCLUDE_DIRECTORIES
        ${APP_SOURCE_DIR}/src/
        ${APP_SOURCE_DIR}/src/apdu/
        ${APP_SOURCE_DIR}/src/swap/
        ${APP_SOURCE_DIR}/src/handler/
        ${APP_SOURCE_DIR}/src/helper/
        ${APP_SOURCE_DIR}/src/token/
        ${APP_SOURCE_DIR}/src/transaction/
        ${APP_SOURCE_DIR}/src/ui/
        ${APP_SOURCE_DIR}/src/ui/action/
        ${BOLOS_SDK}/lib_tlv/use_cases/
        ${CMAKE_SOURCE_DIR}/mock/
        ${CMAKE_SOURCE_DIR}/
    COMPILE_DEFINITIONS
        HAVE_SWAP
)
