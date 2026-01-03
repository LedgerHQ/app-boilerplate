#pragma once

/**
 * UI buffer size constants.
 */
#define MAX_UINT64_STRING_LENGTH 21
#define MAX_VALIDITY_BOUNDARY_STRING_LENGTH 35  // "epoch %u / slot %u" up to 10 digits each
#define MAX_ADA_AMOUNT_STRING_LENGTH 32         // 20 digits + "." + 6 decimals + " ADA"
#define MAX_MINT_SUMMARY_STRING_LENGTH 32       // For mint summary strings (e.g., "2 asset groups")
#define ASSET_FINGERPRINT_HRP_LENGTH 5          // "asset"
#define ASSET_FINGERPRINT_DATA_LENGTH 20        // blake2b-160
#define ASSET_FINGERPRINT_BASE32_LENGTH 32      // ceil(8/5 * 20)
#define MAX_TOKEN_FINGERPRINT_STRING_LENGTH \
    (ASSET_FINGERPRINT_HRP_LENGTH + 1 + 6 + ASSET_FINGERPRINT_BASE32_LENGTH)
#define MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH 70  // TODO: confirm length rationale vs token registry tickers
#define MAX_MINT_AMOUNT_STRING_LENGTH 71        // 70 + 1 for leading sign; TODO: confirm length rationale
#define MAX_WARNING_MESSAGE_LENGTH 128       // For warning/error message text
#define MAX_TX_HASH_DISPLAY_LENGTH 65        // For transaction hash hex display (32 bytes + null)
#define MAX_REFERENCE_SCRIPT_STRING_LENGTH 30  // "Reference script (65535 bytes)"
#define MAX_COLLATERAL_STRING_LENGTH 13        // "return output"
#define MAX_PROFIT_MARGIN_LENGTH 50          // For pool margin "num/den"
#define MAX_VOTE_OPTION_LENGTH 16            // For vote option strings ("Abstain", "Yes", "No")
#define MAX_DREP_OPTION_LENGTH 32            // For DRep option strings ("No Confidence")
