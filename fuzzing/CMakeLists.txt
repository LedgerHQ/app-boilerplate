cmake_minimum_required(VERSION 3.10)

if(${CMAKE_VERSION} VERSION_LESS 3.10)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# project information
project(FuzzTxParser
        VERSION 1.0
	      DESCRIPTION "Fuzzing of transaction parser"
        LANGUAGES C)

# guard against bad build-type strings
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Fuzzer needs to be built with Clang")
endif()

if (NOT DEFINED BOLOS_SDK)
  message(FATAL_ERROR "BOLOS_SDK environment variable not found.")
endif()

# guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

# compatible with ClusterFuzzLite
if (NOT DEFINED ENV{LIB_FUZZING_ENGINE})
	set(COMPILATION_FLAGS_ "-g -Wall -fsanitize=fuzzer,address,undefined")
else()
	set(COMPILATION_FLAGS_ "$ENV{LIB_FUZZING_ENGINE} $ENV{CXXFLAGS}")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

string(REPLACE " " ";" COMPILATION_FLAGS ${COMPILATION_FLAGS_})

include(extra/TxParser.cmake)

add_executable(fuzz_tx_parser fuzz_tx_parser.c)

target_compile_options(fuzz_tx_parser PUBLIC ${COMPILATION_FLAGS})
target_link_options(fuzz_tx_parser PUBLIC ${COMPILATION_FLAGS})
target_link_libraries(fuzz_tx_parser PUBLIC txparser)
