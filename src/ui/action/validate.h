#pragma once

#include <stdbool.h>  // bool

/**
 * Action for address review validation.
 *
 * @param[in] choice
 *   User choice (either approved or rejected).
 *
 */
void validate_address(bool choice);

/**
 * Action for transaction information validation.
 *
 * @param[in] choice
 *   User choice (either approved or rejectd).
 *
 */
void validate_transaction(bool choice);
