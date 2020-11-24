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
 * Function to handle transaction validation on display.
 *
 * @brief user action for transaction validation.
 *
 * @param choice user choice on display.
 *
 */
void ui_action_validate_transaction(bool choice);
