cmake_minimum_required(VERSION 3.20)

project(BoilerPlateFuzzer
        VERSION 1.0
        DESCRIPTION "Ledger App Boilerplate Fuzzer"
        LANGUAGES C)

if(NOT DEFINED BOLOS_SDK)
  message(FATAL_ERROR "BOLOS_SDK must be defined")
endif()

# Exports the `secure_sdk` target and ledger_fuzz_add_app_target().
include(${BOLOS_SDK}/fuzzing/cmake/LedgerAppFuzz.cmake)
ledger_fuzz_setup()

set(APP_SOURCE_DIR ${CMAKE_SOURCE_DIR}/..)

# The whole app minus its hardware entry point, which the harness replaces.
file(GLOB_RECURSE C_SOURCES CONFIGURE_DEPENDS ${APP_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM C_SOURCES ${APP_SOURCE_DIR}/src/app_main.c)

ledger_fuzz_collect_include_dirs(APP_INCLUDE_DIRS ${APP_SOURCE_DIR}/src)
list(APPEND APP_INCLUDE_DIRS ${BOLOS_SDK}/lib_tlv/use_cases ${CMAKE_SOURCE_DIR})

# HAVE_* / APPNAME / ... arrive transitively from secure_sdk
# (make list-defines + macros/add_macros.txt).
ledger_fuzz_add_app_target(
    NAME fuzz_app
    SOURCES ${C_SOURCES}
    INCLUDE_DIRECTORIES ${APP_INCLUDE_DIRS})
