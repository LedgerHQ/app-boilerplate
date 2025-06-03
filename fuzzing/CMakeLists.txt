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

# compatible with ClusterFuzzLite
if(NOT DEFINED ENV{LIB_FUZZING_ENGINE})
  set(COMPILATION_FLAGS
      -Wno-dev
      -fno-common
      -std=gnu99
      -Wall
      -Wextra
      -Wno-main
      -Wno-error=int-conversion
      -Wimplicit-fallthrough
      -Wvla
      -Wundef
      -Wshadow
      -Wformat=2
      -Wformat-security
      -Wwrite-strings
      -ffunction-sections
      -fdata-sections
      -fprofile-instr-generate
      -fcoverage-mapping
      -funsigned-char
      -fshort-enums)

  set(LINK_FLAGS -ffunction-sections -fdata-sections -fprofile-instr-generate
                 -fcoverage-mapping -fuse-ld=lld)
  if(SANITIZER MATCHES "address")
    set(COMPILATION_FLAGS ${COMPILATION_FLAGS}
                          -fsanitize=fuzzer,address,undefined)
    set(LINK_FLAGS ${LINK_FLAGS} -fsanitize=fuzzer,address,undefined)
  elseif(SANITIZER MATCHES "memory")
    set(COMPILATION_FLAGS
        ${COMPILATION_FLAGS} -fsanitize=fuzzer,memory,undefined
        -fsanitize-memory-track-origins -fsanitize=fuzzer-no-link)
    set(LINK_FLAGS ${LINK_FLAGS} -fsanitize=fuzzer,memory,undefined
                   -fsanitize-memory-track-origins -fsanitize=fuzzer-no-link)
  else()
    message(
      FATAL_ERROR
        "Unknown sanitizer type. It must be set to `address` or `memory`.")
  endif()
else()
  set(COMPILATION_FLAGS "$ENV{LIB_FUZZING_ENGINE} $ENV{CFLAGS}")
  set(LINK_FLAGS "$ENV{LIB_FUZZING_ENGINE} $ENV{LDFLAGS}")
  separate_arguments(COMPILATION_FLAGS)
  separate_arguments(LINK_FLAGS)
endif()

set(DEFINES FUZZ APPNAME="Boilerplate")

if(NOT DEFINED BOLOS_SDK)
  message(FATAL_ERROR "BOLOS_SDK must be defined, CMake will exit.")
  return()
else()
  message(${BOLOS_SDK})
endif()

add_subdirectory(${BOLOS_SDK}/fuzzing ${CMAKE_SOURCE_DIR}/build/sdk_build)

file(
  GLOB
  C_SOURCES
  "${CMAKE_SOURCE_DIR}/../src/*.c",
  "${CMAKE_SOURCE_DIR}/../src/apdu/*.c",
  "${CMAKE_SOURCE_DIR}/../src/swap/*.c",
  "${CMAKE_SOURCE_DIR}/../src/handler/*.c",
  "${CMAKE_SOURCE_DIR}/../src/helper/*.c",
  "${CMAKE_SOURCE_DIR}/../src/transaction/*.c",
  "${CMAKE_SOURCE_DIR}/../src/ui/*.c",
  "${CMAKE_SOURCE_DIR}/../src/ui/action/*.c",)
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
target_compile_definitions(code_lib PUBLIC ${DEFINES})
target_link_options(code_lib PUBLIC ${LINK_FLAGS})
target_compile_options(code_lib PUBLIC ${COMPILATION_FLAGS})

# fuzz_dispatcher
add_executable(fuzz_dispatcher "${CMAKE_SOURCE_DIR}/fuzz_dispatcher.c")

target_compile_definitions(fuzz_dispatcher PUBLIC macros)
target_compile_options(fuzz_dispatcher PUBLIC ${COMPILATION_FLAGS})
target_link_options(fuzz_dispatcher PUBLIC ${LINK_FLAGS})
target_link_libraries(fuzz_dispatcher PUBLIC extra code_lib)

# fuzz_tx_parser
add_executable(fuzz_tx_parser "${CMAKE_SOURCE_DIR}/fuzz_tx_parser.c")

target_compile_definitions(fuzz_tx_parser PUBLIC macros)
target_compile_options(fuzz_tx_parser PUBLIC ${COMPILATION_FLAGS})
target_link_options(fuzz_tx_parser PUBLIC ${LINK_FLAGS})
target_link_libraries(fuzz_tx_parser PUBLIC extra code_lib)
