include_guard()
cmake_minimum_required(VERSION 3.14)

if(${CMAKE_VERSION} VERSION_LESS 3.14)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# project information
project(
  BoilerPlateFuzzer
  VERSION 1.0
  DESCRIPTION "App Boilerplate example Fuzzer"
  LANGUAGES C)

set(DEFINES FUZZ APPNAME="Boilerplate")

if(NOT DEFINED BOLOS_SDK)
  message(FATAL_ERROR "BOLOS_SDK must be defined, CMake will exit.")
  return()
else()
  message(${BOLOS_SDK})
endif()

add_subdirectory(${BOLOS_SDK}/fuzzing ${CMAKE_CURRENT_BINARY_DIR}/ledger-secure-sdk EXCLUDE_FROM_ALL)

file(
  GLOB
  C_SOURCES
  "${CMAKE_SOURCE_DIR}/../src/*.c"
  "${CMAKE_SOURCE_DIR}/../src/apdu/*.c"
  "${CMAKE_SOURCE_DIR}/../src/swap/*.c"
  "${CMAKE_SOURCE_DIR}/../src/handler/*.c"
  "${CMAKE_SOURCE_DIR}/../src/helper/*.c"
  "${CMAKE_SOURCE_DIR}/../src/transaction/*.c"
  "${CMAKE_SOURCE_DIR}/../src/ui/*.c"
  "${CMAKE_SOURCE_DIR}/../src/ui/action/*.c"
  "${CMAKE_SOURCE_DIR}/mock/*.c"
  )
list(REMOVE_ITEM C_SOURCES "${CMAKE_SOURCE_DIR}/../src/app_main.c")

add_library(code_lib ${C_SOURCES})

target_include_directories(
  code_lib
  PUBLIC ${CMAKE_SOURCE_DIR}/../src/
         ${CMAKE_SOURCE_DIR}/../src/apdu/
         ${CMAKE_SOURCE_DIR}/../src/swap/
         ${CMAKE_SOURCE_DIR}/../src/handler/
         ${CMAKE_SOURCE_DIR}/../src/helper/
         ${CMAKE_SOURCE_DIR}/../src/transaction/
         ${CMAKE_SOURCE_DIR}/../src/ui/
         ${CMAKE_SOURCE_DIR}/../src/ui/action/)

target_link_libraries(code_lib PUBLIC extra)
target_compile_definitions(code_lib PUBLIC ${DEFINES} FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1)

# fuzz_dispatcher
add_executable(fuzz_dispatcher "${CMAKE_SOURCE_DIR}/fuzz_dispatcher.c")
target_compile_definitions(fuzz_dispatcher PUBLIC macros FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1)
target_link_libraries(fuzz_dispatcher PUBLIC extra code_lib)

# fuzz_tx_parser
add_executable(fuzz_tx_parser "${CMAKE_SOURCE_DIR}/fuzz_tx_parser.c")
target_compile_definitions(fuzz_tx_parser PUBLIC macros FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1)
target_link_libraries(fuzz_tx_parser PUBLIC extra code_lib)
