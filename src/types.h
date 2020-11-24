#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#include "constants.h"
#include "transaction/types.h"
#include "common/bip32.h"

/**
 * Enumeration for the status of IO.
 *
 * @brief status of IO, either READY, RECEIVED or WAITING.
 *
 */
typedef enum {
    READY,     /// ready for new event
    RECEIVED,  /// data received
    WAITING    /// waiting
} io_state_e;

/**
 * Enumeration with expected INS of APDU commands.
 *
 * @brief available INS for APDU command.
 *
 */
typedef enum {
    GET_VERSION = 0x03,     /// version of the application
    GET_APP_NAME = 0x04,    /// name of the application
    GET_PUBLIC_KEY = 0x05,  /// public key of corresponding BIP32 path
    SIGN_TX = 0x06          /// sign transaction with BIP32 path
} command_e;

/**
 * Structure of APDU command to receive.
 *
 * @brief struct within APDU command fields (CLA, INS, P1, P2, Lc, Command data).
 *
 */
typedef struct {
    uint8_t cla;    /// Instruction class
    command_e ins;  /// Instruction code
    uint8_t p1;     /// Instruction parameter 1
    uint8_t p2;     /// Instruction parameter 2
    uint8_t lc;     /// Lenght of command data
    uint8_t *data;  /// Command data
} command_t;

/**
 * Enumeration with parsing state.
 *
 * @brief state of transaction parsing.
 *
 */
typedef enum {
    STATE_NONE,     /// No state
    STATE_PARSED,   /// Transaction data parsed
    STATE_APPROVED  /// Transaction data approved
} state_e;

/**
 * Enumeration with request type to user.
 *
 * @brief request type to user.
 *
 */
typedef enum {
    CONFIRM_ADDRESS,     /// confirm address derived from public key
    CONFIRM_TRANSACTION  /// confirm transaction information
} request_type_e;

/**
 * Structure for public key context information.
 *
 * @brief public key context with chain code.
 *
 */
typedef struct {
    uint8_t raw_public_key[64];
    uint8_t chain_code[32];
} pubkey_ctx_t;

/**
 * Structure for transaction information context.
 *
 * @brief transaction context needed to be signed.
 *
 */
typedef struct {
    uint8_t raw_tx[MAX_TRANSACTION_LEN];
    size_t raw_tx_len;
    transaction_t transaction;
    uint8_t m_hash[32];
    uint8_t signature[MAX_DER_SIG_LEN];
    uint8_t signature_len;
    uint8_t v;
} transaction_ctx_t;

/**
 * Structure for global context.
 *
 * @brief global context used during user request.
 *
 */
typedef struct {
    state_e state;
    union {
        pubkey_ctx_t pk_info;
        transaction_ctx_t tx_info;
    };
    request_type_e req_type;
    uint32_t bip32_path[MAX_BIP32_PATH];
    uint8_t bip32_path_len;
} global_ctx_t;
