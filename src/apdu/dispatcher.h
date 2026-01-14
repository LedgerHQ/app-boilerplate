#pragma once

#include "parser.h"

enum { P1_UNUSED = 0, P2_UNUSED = 0 };

/**
 * Parameter 1 for transaction INIT APDU (new protocol).
 * Signals start of transaction data.
 */
#define P1_TX_INIT 0x00

/**
 * Parameter 1 for transaction data chunks (new protocol).
 * Signals more transaction data chunks to follow.
 */
#define P1_TX_DATA_CHUNK 0x01

/**
 * Parameter 1 for final transaction data chunk (new protocol).
 * Signals last chunk of transaction data - triggers processing.
 */
#define P1_TX_CHUNK_LAST 0x02

/**
 * Parameter 1 for CVote auxiliary data APDUs.
 */
#define P1_TX_AUX_DATA 0x03

/**
 * Parameter 1 for transaction witness signing (legacy protocol).
 * Signals witness request with BIP32 path.
 */
#define P1_TX_SIGN_WITNESS 0x0f

/**
 * Parameter 2 values for CVote auxiliary data APDUs.
 */
#define P2_AUX_DATA_INIT 0x36
#define P2_AUX_DATA_DELEGATION 0x37


/**
 * Dispatch APDU command received to the right handler.
 *
 * @param[in] cmd
 *   Structured APDU command (CLA, INS, P1, P2, Lc, Command data).
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
void apdu_dispatcher(const command_t *cmd);
