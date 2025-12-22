#pragma once

/**
 * UI buffer size constants.
 */
#define MAX_UINT64_STRING_SIZE 21
#define MAX_VALIDITY_BOUNDARY_STRING_SIZE 30  // For epoch/slot formatting (e.g., "epoch 208 / slot 431999")
#define MAX_ADA_AMOUNT_STRING_SIZE 30         // For formatted ADA amounts (e.g., "123.456789 ADA")
#define MAX_WARNING_MESSAGE_SIZE 128       // For warning/error message text
#define MAX_OUTPUT_LABEL_SIZE 32           // For output labels (e.g., "Output 999 Address")
#define MAX_TX_HASH_DISPLAY_SIZE 65        // For transaction hash hex display (32 bytes + null)
