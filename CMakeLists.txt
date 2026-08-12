cmake_minimum_required(VERSION 3.28)

project(app-boilerplate
        VERSION 2.3.2
        LANGUAGES C
)

if(NOT DEFINED ENV{BOLOS_SDK})
    message(FATAL_ERROR "BOLOS_SDK is not set. Point it at a ledger-secure-sdk checkout.")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#######################################################################
#                          App metadata                               #
#######################################################################
# Read by the SDK target profile and by ut_compile_defines.
set(LEDGER_APPVERSION "${PROJECT_VERSION}")
set(ENV{APPNAME} "Boilerplate")

#######################################################################
#                              SDK                                    #
#######################################################################
set(APP_ICON_nanox  ${CMAKE_CURRENT_SOURCE_DIR}/icons/app_boilerplate_14px.gif)
set(APP_ICON_nanos2 ${CMAKE_CURRENT_SOURCE_DIR}/icons/app_boilerplate_14px.gif)
set(APP_ICON_stax   ${CMAKE_CURRENT_SOURCE_DIR}/icons/app_boilerplate_32px.gif)
set(APP_ICON_flex   ${CMAKE_CURRENT_SOURCE_DIR}/icons/app_boilerplate_40px.gif)
set(APP_ICON_apex_p ${CMAKE_CURRENT_SOURCE_DIR}/icons/app_boilerplate_32px_apex.png)

set(ENABLE_BLUETOOTH ON CACHE BOOL "" FORCE)
set(ENABLE_TESTING_SWAP ON CACHE BOOL "" FORCE)
set(ENABLE_NBGL_QRCODE ON CACHE BOOL "" FORCE)
set(ENABLE_TLV_LIBRARY ON CACHE BOOL "" FORCE)
set(ENABLE_PKI_LIBRARY ON CACHE BOOL "" FORCE)

file(GLOB APP_GLYPHS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/glyphs/*)
set(LEDGER_APP_GLYPH_FILES ${APP_GLYPHS} ${APP_ICON_${LEDGER_TARGET}})

add_subdirectory($ENV{BOLOS_SDK} ${CMAKE_BINARY_DIR}/sdk)

if(BUILD_UNIT_TESTS)
    ledger_unit_tests_init()
endif()

#######################################################################
#                          Application                                #
#######################################################################
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/address.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/apdu/dispatcher.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/handler/get_app_name.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/handler/get_public_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/handler/get_version.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/handler/provide_token_info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/handler/sign_tx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/helper/send_reponse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/swap/handle_check_address.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/swap/handle_get_printable_amount.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/swap/handle_swap_sign_transaction.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/token/dynamic_token_info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/token/token_db.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transaction/deserialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transaction/serialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transaction/utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/action/validate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu_nbgl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/nbgl_display_address.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/nbgl_display_transaction.c
)

set(APP_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/apdu
    ${CMAKE_CURRENT_SOURCE_DIR}/src/handler
    ${CMAKE_CURRENT_SOURCE_DIR}/src/helper
    ${CMAKE_CURRENT_SOURCE_DIR}/src/swap
    ${CMAKE_CURRENT_SOURCE_DIR}/src/token
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transaction
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/action
)

add_library(app STATIC ${APP_SOURCES})

target_include_directories(app PUBLIC ${APP_INCLUDE_DIRS})

if(LEDGER_DEVICE_BUILD)
    target_link_libraries(app PUBLIC ledger::sdk)
endif()

target_compile_definitions(app PUBLIC
    APPVERSION="${PROJECT_VERSION}"
    MAJOR_VERSION=${PROJECT_VERSION_MAJOR}
    MINOR_VERSION=${PROJECT_VERSION_MINOR}
    PATCH_VERSION=${PROJECT_VERSION_PATCH}
)

if(BUILD_UNIT_TESTS)
    target_link_libraries(app PRIVATE ledger::target-profile)
    target_include_directories(app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/unit-tests/fakes
        ${UT_INCLUDE_DIRS_SDK}
    )
    target_compile_definitions(app PRIVATE ${UT_COMPILE_DEFS_SDK})
    add_subdirectory(unit-tests)
endif()

#######################################################################
#                          Device binary                              #
#######################################################################
if(LEDGER_DEVICE_BUILD)
    # The SDK's own find_package(Python3) happens in lib_nbgl's scope, not here.
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    # loadApp --icon wants a hex-encoded bitmap, not a path.
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${LEDGER_SDK_ROOT}/lib_nbgl/tools/icon2glyph.py
                --hexbitmap ${CMAKE_BINARY_DIR}/icon.hex ${APP_ICON_${LEDGER_TARGET}}
        COMMAND_ERROR_IS_FATAL ANY)
    file(READ ${CMAKE_BINARY_DIR}/icon.hex APP_ICON_HEX)
    string(STRIP "${APP_ICON_HEX}" APP_ICON_HEX)

    # Same header the C code reads, so the id cannot drift from the target.
    file(STRINGS ${LEDGER_SDK_ROOT}/target/${LEDGER_TARGET}/include/bolos_target.h
         _target_id_line REGEX "define[ \t]+TARGET_ID")
    string(REGEX MATCH "0x[0-9a-fA-F]+" LEDGER_TARGET_ID "${_target_id_line}")

    add_executable(app.elf
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app_main.c
        ${LEDGER_SDK_ROOT}/lib_standard_app/main.c
    )

    target_link_libraries(app.elf PRIVATE app ledger::sdk)

    # Same post-processing as Makefile.rules_generic.
    add_custom_command(TARGET app.elf POST_BUILD
        # print sections sizes
        COMMAND size -A $<TARGET_FILE:app.elf>
        # generate hex
        COMMAND llvm-objcopy -O ihex -S $<TARGET_FILE:app.elf> ${CMAKE_BINARY_DIR}/app.hex
        # generate map
        COMMAND llvm-objdump -h -t $<TARGET_FILE:app.elf> > ${CMAKE_BINARY_DIR}/app.map
        # generate apdu
        # former Makefile command : ${Python3_EXECUTABLE} -m ledgerblue.loadApp $(APP_LOAD_PARAMS) --offline $(BIN_DIR)/app.apdu
        # XXX to be factorized and moved in SDK.
        COMMAND ${Python3_EXECUTABLE} -m ledgerblue.loadApp
                --offline ${CMAKE_BINARY_DIR}/app.apdu
                --fileName ${CMAKE_BINARY_DIR}/app.hex
                --appFlags 0x200
                --curve secp256k1
                --path "44'/1'"
                --targetId ${LEDGER_TARGET_ID}
                --appName "Boilerplate"
                --appVersion "${PROJECT_VERSION}"
                --dataSize 0x200
                --icon ${APP_ICON_HEX}
                --tlv
                --apiLevel ${LEDGER_API_LEVEL}
                --delete
        COMMENT "Generating app.hex, app.map and app.apdu"

        VERBATIM)
endif()
