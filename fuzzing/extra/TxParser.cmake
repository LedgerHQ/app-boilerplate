# specify C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

add_library(txparser
    ${BOLOS_SDK}/lib_standard_app/format.c
    ${BOLOS_SDK}/lib_standard_app/buffer.c
    ${BOLOS_SDK}/lib_standard_app/read.c
    ${BOLOS_SDK}/lib_standard_app/varint.c
    ${BOLOS_SDK}/lib_standard_app/bip32.c
    ${BOLOS_SDK}/lib_standard_app/write.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/deserialize.c
)

set_target_properties(txparser PROPERTIES SOVERSION 1)

target_include_directories(txparser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction
    ${BOLOS_SDK}/lib_standard_app
)

target_compile_definitions(txparser PRIVATE FUZZ PRINTF=)
target_compile_options(txparser PRIVATE -Wall -Wextra -Wno-unused-function -pedantic)
