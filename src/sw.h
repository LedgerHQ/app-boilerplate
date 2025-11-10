#pragma once

/**
 * Status word for success.
 */
#define SW_OK 0x9000
/**
 * Status word for denied by user.
 */
#define SW_DENY 0x6985
/**
 * Status word for incorrect P1 or P2.
 */
#define SW_WRONG_P1P2 0x6A86
/**
 * Status word for either wrong Lc or length of APDU command less than 5.
 */
#define SW_WRONG_DATA_LENGTH 0x6A87
/**
 * Status word for unknown command with this INS.
 */
#define SW_INS_NOT_SUPPORTED 0x6D00
/**
 * Status word for instruction class is different than CLA.
 */
#define SW_CLA_NOT_SUPPORTED 0x6E00
/**
 * Status word for wrong response length (buffer too small or too big).
 */
#define SW_WRONG_RESPONSE_LENGTH 0xB000
/**
 * Status word for fail to display BIP32 path.
 */
#define SW_DISPLAY_BIP32_PATH_FAIL 0xB001
/**
 * Status word for fail to display address.
 */
#define SW_DISPLAY_ADDRESS_FAIL 0xB002
/**
 * Status word for fail to display amount.
 */
#define SW_DISPLAY_AMOUNT_FAIL 0xB003
/**
 * Status word for wrong transaction length.
 */
#define SW_WRONG_TX_LENGTH 0xB004
/**
 * Status word for fail of transaction parsing (general).
 */
#define SW_TX_PARSING_FAIL 0xB005
/**
 * Status word for fail of transaction hash.
 */
#define SW_TX_HASH_FAIL 0xB006
/**
 * Status word for fail of parsing transaction inputs.
 */
#define SW_TX_PARSING_FAIL_INPUTS 0xB020
/**
 * Status word for fail of parsing transaction outputs.
 */
#define SW_TX_PARSING_FAIL_OUTPUTS 0xB021
/**
 * Status word for fail of parsing transaction fee.
 */
#define SW_TX_PARSING_FAIL_FEE 0xB022
/**
 * Status word for fail of parsing transaction TTL.
 */
#define SW_TX_PARSING_FAIL_TTL 0xB023
/**
 * Status word for invalid inclusion flag (must be ITEM_INCLUDED_YES or ITEM_INCLUDED_NO).
 */
#define SW_TX_PARSING_FAIL_INCLUSION_FLAG 0xB024
/**
 * Status word for bad state.
 */
#define SW_BAD_STATE 0xB007
/**
 * Status word for signature fail.
 */
#define SW_SIGNATURE_FAIL 0xB008
/**
 * Status word for insufficient memory.
 */
#define SW_INSUFFICIENT_MEMORY 0xB009


// TODO
#define SW_OPCERT_PARSING_FAIL 0xB011
#define SW_WRONG_OPCERT_LENGTH 0xB012
#define SW_BIP44_PATH_PARSING_FAIL 0xB013
#define SW_IO_FAIL 0xB014

// TODO these are from previous app, but should perhaps be replaced
enum {
    // Successful responses
    SUCCESS = 0x9000,

    // Invalid INS in swap mode
    ERR_SWAP_FAIL = 0x6001,

    // Start of error which trigger automatic response
    // Note that any such error will reset
    // multi-APDU exchange
    _ERR_AUTORESPOND_START = 0x6E00,

    // Bad request header
    ERR_MALFORMED_REQUEST_HEADER = 0x6E01,
    // Unknown CLA
    ERR_BAD_CLA = 0x6E02,
    // Unknown INS
    ERR_UNKNOWN_INS = 0x6E03,
    // attempt to change INS while the current call was not finished
    ERR_STILL_IN_CALL = 0x6E04,
    // P1 or P2 is invalid
    ERR_INVALID_REQUEST_PARAMETERS = 0x6E05,
    // Request is not valid in the context of previous calls
    ERR_INVALID_STATE = 0x6E06,
    // Some part of request data is invalid (or unknown to this app)
    // (includes not enough data and too much data)
    ERR_INVALID_DATA = 0x6E07,

    // User rejected the action
    ERR_REJECTED_BY_USER = 0x6E09,
    // Ledger security policy rejected the action
    ERR_REJECTED_BY_POLICY = 0x6E10,

    // Pin screen
    ERR_DEVICE_LOCKED = 0x6E11,

    // end of errors which trigger automatic response
    _ERR_AUTORESPOND_END = 0x6E13,

    // Errors below SHOULD NOT be returned to the client
    // Instead, leaking these to the main() scope
    // means unexpected programming error
    // and we should stop further processing
    // to avoid exploits

    // Internal errors
    ERR_ASSERT = 0x4700,
    ERR_NOT_IMPLEMENTED = 0x4701,

    // stream
    ERR_NOT_ENOUGH_INPUT = 0x4710,
    ERR_DATA_TOO_LARGE = 0x4711,

    // cbor
    ERR_UNEXPECTED_TOKEN = 0x4720,

    // Explicit return value to not send any response from main loop
    ERR_NO_RESPONSE = 0x0000,
};
