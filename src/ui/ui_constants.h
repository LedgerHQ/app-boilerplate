#pragma once

/**
 * UI buffer size constants.
 */
#define MAX_UINT64_STRING_LENGTH 21
#define MAX_VALIDITY_BOUNDARY_STRING_LENGTH 30  // For epoch/slot formatting (e.g., "epoch 208 / slot 431999")
#define MAX_ADA_AMOUNT_STRING_LENGTH 30         // For formatted ADA amounts (e.g., "123.456789 ADA")
#define MAX_MINT_SUMMARY_STRING_LENGTH 32       // For mint summary strings (e.g., "2 asset groups")
#define MAX_TOKEN_FINGERPRINT_STRING_LENGTH 80  // For asset fingerprint text
#define MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH 70  // For output token amount text
#define MAX_MINT_AMOUNT_STRING_LENGTH 64        // For mint token amount text
#define MAX_WARNING_MESSAGE_LENGTH 128       // For warning/error message text
#define MAX_OUTPUT_LABEL_LENGTH 32           // For output labels (e.g., "Output 999 Address")
#define MAX_TX_HASH_DISPLAY_LENGTH 65        // For transaction hash hex display (32 bytes + null)
