#pragma once

#include <stdbool.h>  // bool

/**
 * Callback to reuse action with approve/reject in step FLOW.
 */
typedef void (*action_validate_cb)(bool);

/**
 * launch transaction review.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
int ui_display_transaction(void);

/**
 * launch address review.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
int ui_display_address(void);

