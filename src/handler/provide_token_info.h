/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#pragma once

#include "buffer.h"

/**
 * @brief Handle PROVIDE_TOKEN_INFO APDU command (INS 0x22)
 *
 * IMPORTANT: This is an ADVANCED feature requiring Ledger team coordination.
 * The hardcoded tokens in src/token/token_db.c is a simpler method to develop.
 *
 * Receives and validates a TLV-encoded dynamic token descriptor from CAL
 * (Crypto Asset List). The TLV payload contains token metadata (ticker, decimals)
 * signed by Ledger's trusted CAL backend, allowing runtime token database knowledge.
 *
 * TLV Structure common for all Ledger applications Dynamic Token:
 *   - Tag 0x01: STRUCTURE_TYPE (1 byte) = 0x90
 *   - Tag 0x02: VERSION (1 byte) = 0x01
 *   - Tag 0x03: COIN_TYPE (4 bytes) = SLIP-44 coin type (see constants.h)
 *   - Tag 0x04: APPLICATION_NAME (variable, checked against APPNAME)
 *   - Tag 0x05: TICKER (variable, ≤50 chars) = e.g., "USDC"
 *   - Tag 0x06: MAGNITUDE (1 byte) = decimals (e.g., 12)
 *   - Tag 0x07: TUID (variable) = app-specific sub-TLV with token address
 *   - Tag 0x08: SIGNATURE (64-72 bytes) = ECDSA signature over tags 0x01-0x07
 *
 * TUID sub-TLV (Tag 0x07) Boilerplate custom example:
 *   - Tag 0x10: TOKEN_ADDRESS (32 bytes) = token contract address
 *
 * Security:
 *   - Signature verified using PKI with CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META (0x08)
 *   - Coin type must match configured SLIP-44 value (see constants.h)
 *
 * @param[in] cdata Buffer containing TLV payload
 * @return Status word (SWO_SUCCESS on success, SW_INVALID_DYNAMIC_TOKEN on failure)
 */
int handler_provide_token_info(buffer_t *cdata);
