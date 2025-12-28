#pragma once

#include <stddef.h>  // size_t
#include <stdbool.h>
#include <stdint.h>  // uint*_t
#include "cardano_constants.h"
#include "addressUtilsShelley.h"
#include "bip44.h"      // for bip44_path_t


#define MAX_ADDRESS_SIZE              128
#define MAX_HUMAN_ADDRESS_SIZE        150


bool isValidStakingChoice(staking_data_source_t stakingDataSource);

/**
 * Structure for derive address information context.
 */
typedef struct {
    uint16_t responseReadyMagic;
    addressParams_t addressParams;
    struct {
        uint8_t buffer[MAX_ADDRESS_SIZE];
        size_t size;
    } address;
    int ui_step;
} ins_derive_address_ctx_t;
