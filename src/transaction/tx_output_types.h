#pragma once

#include "utils/list.h"
#include "txHashBuilder/txHashBuilder.h"

// Output list item with flist node
// Contains destination and amount
typedef struct {
    s_flist_node node;      /// flist node for linked list
    struct {
        tx_output_destination_storage_t destination;
        uint64_t adaAmount;
    } output_data;
} tx_output_list_item_t;
