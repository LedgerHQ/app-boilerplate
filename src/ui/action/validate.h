#pragma once

#include <stdbool.h>  // bool

/**
 * Function to handle public key validation on display.
 *
 * @brief user action for public key validation.
 *
 * @param choice user choice on display.
 *
 */
void ui_action_validate_pubkey(bool choice);

/**
 * Function to handle amount validation on display.
 *
 * @brief user action for amount validation.
 *
 * @param choice user choice on display.
 *
 */
void ui_action_validate_amount(bool choice);
