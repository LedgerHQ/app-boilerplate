# Included from the app root CMakeLists.txt, which owns the SDK subdirectory,
# LedgerUT and the `app` coverage library.
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
    MAJOR_VERSION=${PROJECT_VERSION_MAJOR}
    MINOR_VERSION=${PROJECT_VERSION_MINOR}
    PATCH_VERSION=${PROJECT_VERSION_PATCH}
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
    MOCK_HEADERS  ${LEDGER_SDK_ROOT}/lib_nbgl/include/nbgl_use_case.h
                  ${LEDGER_SDK_ROOT}/include/os_nvm.h
                  ${LEDGER_SDK_ROOT}/include/os_task.h
    INCLUDE_DIRS  ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS  ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME          test_nbgl_display_address
    SOURCES       ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/nbgl_display_address.c
    MOCK_HEADERS  ${LEDGER_SDK_ROOT}/lib_nbgl/include/nbgl_use_case.h
                  ${LEDGER_SDK_ROOT}/lib_standard_app/format.h
    INCLUDE_DIRS  ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS  ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME          test_nbgl_display_transaction
    SOURCES       ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/nbgl_display_transaction.c
    MOCK_HEADERS  ${LEDGER_SDK_ROOT}/lib_nbgl/include/nbgl_use_case.h
                  ${LEDGER_SDK_ROOT}/lib_standard_app/format.h
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
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/include/os.h
                   ${LEDGER_SDK_ROOT}/include/cx.h
                   ${LEDGER_SDK_ROOT}/lib_cxng/include/libcxng.h
                   ${LEDGER_SDK_ROOT}/lib_cxng/include/lcx_sha3.h
                   ${LEDGER_SDK_ROOT}/include/ledger_assert_internals.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_dispatcher
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/apdu/dispatcher.c
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_standard_app/buffer.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/io.h
                   ${LEDGER_SDK_ROOT}/include/ledger_assert_internals.h
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
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_standard_app/io.h
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
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_standard_app/io.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_get_app_name
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/get_app_name.c
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_standard_app/io.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_get_public_key
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/get_public_key.c
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_standard_app/io.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/buffer.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_handle_check_address
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/swap/handle_check_address.c
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_standard_app/buffer.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/format.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_handle_get_printable_amount
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/swap/handle_get_printable_amount.c
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_standard_app/swap_utils.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/format.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_handle_swap_sign_transaction
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/swap/handle_swap_sign_transaction.c
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_standard_app/swap_utils.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/format.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/swap_error_code_helpers.h
                   ${LEDGER_SDK_ROOT}/include/os_task.h
                   ${LEDGER_SDK_ROOT}/include/os_utils.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_tx_parser_unity
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/deserialize.c
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/serialize.c
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_standard_app/buffer.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/write.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/varint.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/../src/transaction/utils.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)

ledger_unit_tests_add_test(
    NAME           test_sign_tx
    SOURCES        ${CMAKE_CURRENT_SOURCE_DIR}/../src/handler/sign_tx.c
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_standard_app/buffer.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/io.h
                   ${LEDGER_SDK_ROOT}/lib_cxng/include/lcx_sha3.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/swap_error_code_helpers.h
                   ${LEDGER_SDK_ROOT}/include/os_task.h
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
    MOCK_HEADERS   ${LEDGER_SDK_ROOT}/lib_tlv/use_cases/tlv_use_case_dynamic_descriptor.h
                   ${LEDGER_SDK_ROOT}/lib_tlv/tlv_library.h
                   ${LEDGER_SDK_ROOT}/lib_standard_app/io.h
    INCLUDE_DIRS   ${UT_INCLUDE_DIRS_SDK} ${UT_INCLUDE_DIRS}
    COMPILE_DEFS   ${UT_COMPILE_DEFS_SDK} ${UT_COMPILE_DEFS}
)
