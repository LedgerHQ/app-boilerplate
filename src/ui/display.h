#pragma once

#include <stdbool.h>  // bool

/**
 * Callback to reuse action with approve/reject in step FLOW.
 */
typedef void (*action_validate_cb)(bool);

/**
 * Function to display address on the device.
 *
 * @brief show address on the device and ask confirmation to export.
 *
 */
int ui_display_address(void);

/**
 * Function to display transaction information on the device.
 *
 * @brief show amount and recipient then ask confirmation to export.
 *
 */
int ui_display_transaction(void);
