#pragma once

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool

/**
 * Handler for SIGN_TX command. Called once all APDU chunks have been
 * reassembled by the dispatcher. Deserializes, hashes, and displays
 * the transaction for user confirmation.
 *
 * @see G_context.bip32_path, G_context.tx_info.raw_transaction,
 * G_context.tx_info.signature and G_context.tx_info.v.
 *
 * @param[in]   is_token_tx
 *  Whether the transaction to sign is a token transaction or not.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
int handler_sign_tx(bool is_token_tx);
