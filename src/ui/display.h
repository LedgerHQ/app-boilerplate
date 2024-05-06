#pragma once

#include <stdbool.h>  // bool

#include "constants.h"
#include "transaction/types.h"

#if defined(TARGET_NANOS) || defined(TARGET_NANOX) || defined(TARGET_NANOS2)
#define ICON_APP_BOILERPLATE C_app_boilerplate_16px
#define ICON_APP_WARNING     C_icon_warning
#elif defined(TARGET_STAX) || defined(TARGET_FLEX)
#define ICON_APP_BOILERPLATE C_app_boilerplate_64px
#define ICON_APP_WARNING     C_Warning_64px
#endif

/**
 * Enumeration with parsing state.
 */
typedef enum {
    UI_RET_APPROVED,
    UI_RET_REJECTED,
    UI_RET_FAILURE
} ui_ret_e;

/**
 * Display address on the device and ask confirmation to export.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
ui_ret_e ui_display_address(const uint8_t raw_public_key[PUBKEY_LEN]);

void ui_display_address_status(ui_ret_e ret);

/**
 * Display transaction information on the device and ask confirmation to sign.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
ui_ret_e ui_display_transaction(const transaction_t *transaction);

void ui_display_transaction_status(ui_ret_e ret);
