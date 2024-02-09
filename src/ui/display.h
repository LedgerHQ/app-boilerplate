#pragma once

#include <stdbool.h>  // bool

#include "constants.h"
#include "transaction/types.h"

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
