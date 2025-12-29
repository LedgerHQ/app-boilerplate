#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    const uint8_t *raw_tx;
    size_t raw_tx_len;
    const char *tx_body_cbor_hex;
    const char *expected_hash_hex;
    uint16_t num_inputs;
    uint16_t num_outputs;
    uint16_t num_witnesses;
    uint16_t num_certificates;
    uint16_t num_withdrawals;
    uint16_t num_mint_asset_groups;
    bool include_ttl;
    bool include_validity_interval_start;
    bool include_aux_data_hash;
    const char *aux_data_hash_hex;
    uint64_t options;
    uint8_t network_id;
    uint32_t protocol_magic;
} tx_fixture_t;
