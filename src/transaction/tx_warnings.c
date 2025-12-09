#include "tx_warnings.h"
#include "memory/mem.h"
#include <string.h>

bool tx_warning_add(tx_warning_list_item_t **list_head, tx_warning_type_t type,
                    uint32_t networkId, uint32_t protocolMagic) {
    // Allocate new warning item
    tx_warning_list_item_t *item = (tx_warning_list_item_t *)app_mem_alloc(sizeof(tx_warning_list_item_t));
    if (item == NULL) {
        // Allocation failed - return error instead of silently skipping
        return false;
    }

    // Initialize warning item
    explicit_bzero(item, sizeof(tx_warning_list_item_t));
    item->type = type;
    item->networkId = networkId;
    item->protocolMagic = protocolMagic;

    // Add to front of list
    item->node.next = (s_flist_node *)*list_head;
    *list_head = item;
    return true;
}

bool tx_warning_list_empty(tx_warning_list_item_t *list_head) {
    return list_head == NULL;
}

/**
 * Cleanup and free all warning list items
 *
 * @param list_head pointer to head of warning list
 */
void tx_warning_list_cleanup(tx_warning_list_item_t **list_head) {
    if (list_head == NULL) {
        return;
    }

    tx_warning_list_item_t *current = *list_head;
    while (current != NULL) {
        tx_warning_list_item_t *next = (tx_warning_list_item_t *)current->node.next;
        app_mem_free(current);
        current = next;
    }
    *list_head = NULL;
}

const char* tx_warning_get_message(tx_warning_type_t type) {
    switch (type) {
        case TX_WARNING_NETWORK_UNUSUAL:
            return "Unusual network detected";
        case TX_WARNING_NETWORK_ID_UNVERIFIABLE:
            return "Network ID cannot be verified";
        case TX_WARNING_HIGH_FEE:
            return "Transaction fee is unusually high";
        default:
            return "Unknown warning";
    }
}
