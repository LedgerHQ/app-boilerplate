#pragma once

/**
 * Instruction class of the Boilerplate application.
 */
#define CLA 0xD7

/**
 * Length of APPNAME variable in the Makefile.
 */
#define APPNAME_LEN (sizeof(APPNAME) - 1)

/**
 * Maximum length of MAJOR_VERSION || MINOR_VERSION || PATCH_VERSION.
 */
#define APPVERSION_LEN 3

/**
 * Maximum length of application name.
 */
#define MAX_APPNAME_LEN 64

/**
 * Maximum transaction length (bytes).
 */
#define MAX_TRANSACTION_LEN 510

/**
 * Transaction buffer size for dynamic allocation (bytes).
 * Note: Must be significantly less than SIZE_MEM_BUFFER in mem.c to account for:
 * - HEAP_HEADER_SIZE (~160 bytes for heap metadata)
 * - Chunk headers (4-8 bytes per allocation)
 * - Alignment requirements (8-byte alignment)
 * Maximum tested working size is 14KB from a 24KB pool TODO
 */
#define TX_BUFFER_SIZE (16 * 1024)

/**
 * Maximum signature length (bytes).
 */
#define MAX_DER_SIG_LEN 72

/**
 * Exponent used to convert mBOL to BOL unit (N BOL = N * 10^3 mBOL).
 */
#define EXPONENT_SMALLEST_UNIT 3

#define MAX_UINT64_STRING_SIZE 21

/**
 * Buffer sizes for UI display strings.
 */
#define MAX_ADA_AMOUNT_STRING_SIZE 30      // For formatted ADA amounts (e.g., "123.456789 ADA")
#define MAX_WARNING_MESSAGE_SIZE 128       // For warning/error message text
#define MAX_OUTPUT_LABEL_SIZE 32           // For output labels (e.g., "Output 999 Address"); supports up to 999 outputs
#define MAX_AMOUNT_DISPLAY_SIZE 40         // For formatted amount display with currency (e.g., "BOL 123.456789")
#define MAX_TX_HASH_DISPLAY_SIZE 65        // For transaction hash hex display (32 bytes = 64 hex chars + null terminator)

/**
 * Item inclusion flags (for optional transaction fields).
 */
enum { ITEM_INCLUDED_NO = 1, ITEM_INCLUDED_YES = 2 };

/**
 * Transaction options flags.
 */
enum {
    TX_OPTIONS_TAG_CBOR_SETS = 1,  // Whether to tag CBOR sets in transaction hash
};
