#pragma once

#include <stdint.h>
#include "utils/list.h"

// Warning types for transaction signing
typedef enum {
    TX_WARNING_NETWORK_UNUSUAL = 1,          // Network ID or protocol magic is unusual
    TX_WARNING_NETWORK_ID_UNVERIFIABLE = 2,  // Cannot verify network ID from tx elements
} tx_warning_type_t;

// Warning list item with flist node
typedef struct {
    s_flist_node node;           // flist node for linked list
    tx_warning_type_t type;      // warning type
    uint32_t networkId;          // network ID for context
    uint32_t protocolMagic;      // protocol magic for context
} tx_warning_list_item_t;

// Helper to add warning to list
void tx_warning_add(tx_warning_list_item_t **list_head, tx_warning_type_t type,
                   uint32_t networkId, uint32_t protocolMagic);

// Check if warnings list is empty
bool tx_warning_list_empty(tx_warning_list_item_t *list_head);

// Get warning message for display
const char* tx_warning_get_message(tx_warning_type_t type);
