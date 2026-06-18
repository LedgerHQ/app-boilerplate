cmake_minimum_required(VERSION 3.28)
project(UNIT_TESTS
    VERSION 0.1
    DESCRIPTION "Unit tests for Ledger Nano application"
    LANGUAGES C
)

#######################################################################
#                        Setup SDK & LedgerUT                         #
#######################################################################
if(NOT DEFINED ENV{BOLOS_SDK})
    message(FATAL_ERROR "BOLOS_SDK environment variable is not set. Please set it to the path of the Ledger Secure SDK.")
endif()

set(SDK_DIR $ENV{BOLOS_SDK})
list(APPEND CMAKE_MODULE_PATH "${SDK_DIR}/cmake")
include(LedgerUT)

#######################################################################
#             Setup app specific includes & compile defs              #
#######################################################################
set(UT_INCLUDE_DIRS
    ${CMAKE_BINARY_DIR}/mocks/ # XXX this one might to be moved in SDK, TBD
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/apdu
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/helper
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/swap
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/token
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/action
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/
)

set(UT_COMPILE_DEFS
    APPNAME="Boilerplate"
    APPVERSION="9.9.9"
)

#######################################################################
#                           LedgerUT Setup                            #
#######################################################################
# init ledger_unit_tests environment
ledger_unit_tests_init()

# build all app sources but app_main.c (coverage baseline; not linked by tests)
ledger_unit_tests_find_sources()

# build all app sources so all sources have a .gcno file to init coverage %
ledger_unit_tests_build_app_sources_as_lib(
    SOURCES      ${APP_SOURCES}
    INCLUDE_DIRS ${UT_INCLUDE_DIRS}
    COMPILE_DEFS ${UT_COMPILE_DEFS}
)

#######################################################################
#                        LedgerUT test suites                         #
#######################################################################
ledger_unit_tests_add_test(
    NAME          test_template
)

ledger_unit_tests_add_test(
    NAME          test_menu_nbgl
    SOURCES       ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/menu_nbgl.c
    MOCK_HEADERS  ${SDK_DIR}/lib_nbgl/include/nbgl_use_case.h
                  ${SDK_DIR}/include/os_nvm.h
                  ${SDK_DIR}/include/os_task.h
    INCLUDE_DIRS  ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS  ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME          test_nbgl_display_address
    SOURCES       ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/nbgl_display_address.c
    MOCK_HEADERS  ${SDK_DIR}/lib_nbgl/include/nbgl_use_case.h
                  ${SDK_DIR}/lib_standard_app/format.h
    INCLUDE_DIRS  ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS  ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME          test_nbgl_display_transaction
    SOURCES       ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/nbgl_display_transaction.c
    MOCK_HEADERS  ${SDK_DIR}/lib_nbgl/include/nbgl_use_case.h
                  ${SDK_DIR}/lib_standard_app/format.h
    INCLUDE_DIRS  ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS  ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME          test_validate
    SOURCES       ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/action/validate.c
    INCLUDE_DIRS  ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS  ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_address
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/address.c
    MOCK_HEADERS   ${SDK_DIR}/include/os.h
                   ${SDK_DIR}/include/cx.h
                   ${SDK_DIR}/lib_cxng/include/libcxng.h
                   ${SDK_DIR}/lib_cxng/include/lcx_sha3.h
                   ${SDK_DIR}/include/ledger_assert_internals.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_dispatcher
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/apdu/dispatcher.c
    MOCK_HEADERS   ${SDK_DIR}/lib_standard_app/buffer.h
                   ${SDK_DIR}/lib_standard_app/io.h
                   ${SDK_DIR}/include/ledger_assert_internals.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/get_version.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/get_app_name.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/get_public_key.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/sign_tx.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/provide_token_info.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_send_response
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/helper/send_reponse.c
    MOCK_HEADERS   ${SDK_DIR}/lib_standard_app/io.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_utils
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/utils.c
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_dynamic_token_info
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/token/dynamic_token_info.c
    MOCK_HEADERS   ${CMAKE_CURRENT_SOURCE_DIR}/../src/token/token_db.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_token_db
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/token/token_db.c
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_get_version
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/get_version.c
    MOCK_HEADERS   ${SDK_DIR}/lib_standard_app/io.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_get_app_name
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/get_app_name.c
    MOCK_HEADERS   ${SDK_DIR}/lib_standard_app/io.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_get_public_key
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/get_public_key.c
    MOCK_HEADERS   ${SDK_DIR}/lib_standard_app/io.h
                   ${SDK_DIR}/lib_standard_app/buffer.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_handle_check_address
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/swap/handle_check_address.c
    MOCK_HEADERS   ${SDK_DIR}/lib_standard_app/buffer.h
                   ${SDK_DIR}/lib_standard_app/format.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_handle_get_printable_amount
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/swap/handle_get_printable_amount.c
    MOCK_HEADERS   ${SDK_DIR}/lib_standard_app/swap_utils.h
                   ${SDK_DIR}/lib_standard_app/format.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_handle_swap_sign_transaction
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/swap/handle_swap_sign_transaction.c
    MOCK_HEADERS   ${SDK_DIR}/lib_standard_app/swap_utils.h
                   ${SDK_DIR}/lib_standard_app/format.h
                   ${SDK_DIR}/lib_standard_app/swap_error_code_helpers.h
                   ${SDK_DIR}/include/os_task.h
                   ${SDK_DIR}/include/os_utils.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_tx_parser_unity
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/deserialize.c
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/serialize.c
    MOCK_HEADERS   ${SDK_DIR}/lib_standard_app/buffer.h
                   ${SDK_DIR}/lib_standard_app/write.h
                   ${SDK_DIR}/lib_standard_app/varint.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/utils.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_sign_tx
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/sign_tx.c
    MOCK_HEADERS   ${SDK_DIR}/lib_standard_app/buffer.h
                   ${SDK_DIR}/lib_standard_app/io.h
                   ${SDK_DIR}/lib_cxng/include/lcx_sha3.h
                   ${SDK_DIR}/lib_standard_app/swap_error_code_helpers.h
                   ${SDK_DIR}/include/os_task.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/deserialize.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/action/validate.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/display.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/swap/handle_swap.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_provide_token_info
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/provide_token_info.c
    MOCK_HEADERS   ${SDK_DIR}/lib_tlv/use_cases/tlv_use_case_dynamic_descriptor.h
                   ${SDK_DIR}/lib_tlv/tlv_library.h
                   ${SDK_DIR}/lib_standard_app/io.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)
