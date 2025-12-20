#pragma once

typedef enum {
    TX_STATE_NONE,         /// idle
    TX_STATE_CHUNKS,       /// receiving transaction chunks
    TX_STATE_RECEIVED,     /// all chunks received, waiting to parse
    TX_STATE_PARSED,       /// transaction parsed, ready for hashing
    TX_STATE_HASHED,       /// hash computed, UI plan ready
    TX_STATE_UI_PREPARED,  /// UI strings prepared
    TX_STATE_APPROVED      /// user approved, waiting for witnesses
} tx_state_e;

