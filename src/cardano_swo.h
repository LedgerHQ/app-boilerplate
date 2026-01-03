#pragma once

// SDK status words (ISO 7816-4 standard)
#include <status_words.h>

// Cardano app-specific status words (0xB0XX range)
// TODO this perhaps needs to be redesigned, not sure if the range is valid according to the ISO above
#define SWO_INVALID_TX_LENGTH 0xB004
#define SWO_TX_PARSING_FAIL 0xB005
#define SWO_WRONG_TX_INIT_APDU_DATA 0xB006  // malformed TX INIT APDU structure
#define SWO_BAD_STATE 0xB007
#define SWO_SIGNATURE_FAIL 0xB008
#define SWO_OPCERT_PARSING_FAIL_KES_KEY 0xB010
#define SWO_OPCERT_PARSING_FAIL_KES_PERIOD 0xB011
#define SWO_OPCERT_PARSING_FAIL_ISSUE_COUNTER 0xB012
#define SWO_OPCERT_PARSING_FAIL_POOL_KEY_PATH 0xB013
#define SWO_INVALID_OPCERT_LENGTH 0xB014
#define SWO_BIP44_PATH_PARSING_FAIL 0xB015
// Transaction body field parsing errors
// Organized by CBOR key as per Cardano CDDL
#define SWO_TX_PARSING_FAIL_INPUTS 0xB020              // key 0
#define SWO_TX_PARSING_FAIL_OUTPUTS 0xB021             // key 1
#define SWO_TX_PARSING_FAIL_FEE 0xB022                 // key 2
#define SWO_TX_PARSING_FAIL_TTL 0xB023                 // key 3
#define SWO_TX_PARSING_FAIL_CERTIFICATES 0xB024        // key 4
#define SWO_TX_PARSING_FAIL_WITHDRAWALS 0xB025         // key 5
#define SWO_TX_PARSING_FAIL_AUX_DATA_HASH 0xB027       // key 7
#define SWO_TX_PARSING_FAIL_VALIDITY_INTERVAL_START 0xB028  // key 8
#define SWO_TX_PARSING_FAIL_MINT 0xB029                // key 9
#define SWO_TX_PARSING_FAIL_SCRIPT_DATA_HASH 0xB02B    // key 11
#define SWO_TX_PARSING_FAIL_COLLATERAL_INPUTS 0xB02D   // key 13
#define SWO_TX_PARSING_FAIL_REQUIRED_SIGNERS 0xB02E    // key 14
#define SWO_TX_PARSING_FAIL_COLLATERAL_OUTPUT 0xB030   // key 16
#define SWO_TX_PARSING_FAIL_TOTAL_COLLATERAL 0xB031    // key 17
#define SWO_TX_PARSING_FAIL_REFERENCE_INPUTS 0xB032    // key 18
#define SWO_TX_PARSING_FAIL_VOTING_PROCEDURES 0xB033   // key 19
#define SWO_TX_PARSING_FAIL_TREASURY 0xB035            // key 21
#define SWO_TX_PARSING_FAIL_DONATION 0xB036            // key 22

// Network/Protocol validation errors
#define SWO_INVALID_NETWORK_ID 0xB037                  // network ID mismatch
#define SWO_INVALID_PROTOCOL_MAGIC 0xB038              // protocol magic mismatch

// Transaction structure errors
#define SWO_TX_PARSING_FAIL_INCLUSION_FLAG 0xB039      // optional field flag error
#define SWO_TX_PARSING_FAIL_BUFFER_NOT_FULLY_CONSUMED 0xB03A  // extra data in buffer
#define SWO_TX_PARSING_FAIL_CANONICAL_ORDER 0xB03B     // CBOR canonical ordering

// UI errors
#define SWO_UI_PAIRS_EXCEED_CAPABILITY 0xB03C          // NBGL display limit exceeded
