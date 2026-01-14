#pragma once

// SDK status words (ISO 7816-4 standard)
#include <status_words.h>

typedef enum {
    // Cardano app-specific status words (0x6BXX range)
    // ISO 7816-4 compliant: 0x6BXX is standard "proprietary" range for wrong parameters
    SWO_INVALID_TX_LENGTH = 0x6B00,
    SWO_TX_PARSING_FAIL = 0x6B01,
    SWO_WRONG_TX_INIT_APDU_DATA = 0x6B02,  // malformed TX INIT APDU structure
    SWO_BAD_STATE = 0x6B03,
    SWO_SIGNATURE_FAIL = 0x6B04,
    SWO_BIP44_PATH_PARSING_FAIL = 0x6B05,
    SWO_OPCERT_PARSING_FAIL_KES_KEY = 0x6B10,
    SWO_OPCERT_PARSING_FAIL_KES_PERIOD = 0x6B11,
    SWO_OPCERT_PARSING_FAIL_ISSUE_COUNTER = 0x6B12,
    SWO_OPCERT_PARSING_FAIL_POOL_KEY_PATH = 0x6B13,
    SWO_INVALID_OPCERT_LENGTH = 0x6B14,
    // Transaction body field parsing errors
    // Organized by CBOR key as per Cardano CDDL: error = 0x6B20 + CBOR_KEY
    SWO_TX_PARSING_FAIL_INPUTS = 0x6B20,              // key 0
    SWO_TX_PARSING_FAIL_OUTPUTS = 0x6B21,             // key 1
    SWO_TX_PARSING_FAIL_FEE = 0x6B22,                 // key 2
    SWO_TX_PARSING_FAIL_TTL = 0x6B23,                 // key 3
    SWO_TX_PARSING_FAIL_CERTIFICATES = 0x6B24,        // key 4
    SWO_TX_PARSING_FAIL_WITHDRAWALS = 0x6B25,         // key 5
    SWO_TX_PARSING_FAIL_AUX_DATA_HASH = 0x6B27,       // key 7
    SWO_TX_PARSING_FAIL_VALIDITY_INTERVAL_START = 0x6B28,  // key 8
    SWO_TX_PARSING_FAIL_MINT = 0x6B29,                // key 9
    SWO_TX_PARSING_FAIL_SCRIPT_DATA_HASH = 0x6B2B,    // key 11
    SWO_TX_PARSING_FAIL_COLLATERAL_INPUTS = 0x6B2D,   // key 13
    SWO_TX_PARSING_FAIL_REQUIRED_SIGNERS = 0x6B2E,    // key 14
    SWO_TX_PARSING_FAIL_COLLATERAL_OUTPUT = 0x6B30,   // key 16
    SWO_TX_PARSING_FAIL_TOTAL_COLLATERAL = 0x6B31,    // key 17
    SWO_TX_PARSING_FAIL_REFERENCE_INPUTS = 0x6B32,    // key 18
    SWO_TX_PARSING_FAIL_VOTING_PROCEDURES = 0x6B33,   // key 19
    SWO_TX_PARSING_FAIL_TREASURY = 0x6B35,            // key 21
    SWO_TX_PARSING_FAIL_DONATION = 0x6B36,            // key 22

    // Network/Protocol validation errors
    SWO_INVALID_NETWORK_ID = 0x6B37,                  // network ID mismatch
    SWO_INVALID_PROTOCOL_MAGIC = 0x6B38,              // protocol magic mismatch

    // Transaction structure errors
    SWO_TX_PARSING_FAIL_INCLUSION_FLAG = 0x6B39,      // optional field flag error
    SWO_TX_PARSING_FAIL_BUFFER_NOT_FULLY_CONSUMED = 0x6B3A,  // extra data in buffer
    SWO_TX_PARSING_FAIL_CANONICAL_ORDER = 0x6B3B,     // CBOR canonical ordering
} cardano_status_word_t;
