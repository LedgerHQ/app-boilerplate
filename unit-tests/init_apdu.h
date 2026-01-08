#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "../src/utils/utils.h"  // for ITEM_INCLUDED_*
#include "../src/transaction/tx_aux_data_types.h"

typedef struct {
    uint64_t options;
    uint8_t networkId;
    uint32_t protocolMagic;
    uint8_t signingMode;
    uint16_t numInputs;
    uint16_t numOutputs;
    bool includeTtl;
    uint16_t numCertificates;
    uint16_t numWithdrawals;
    bool includeAuxData;
    aux_data_type_t auxDataType;
    const uint8_t* auxDataHash;
    size_t auxDataHashLen;
    bool includeValidityIntervalStart;
    uint16_t numMintAssetGroups;
    bool includeScriptDataHash;
    uint16_t numCollateralInputs;
    uint16_t numRequiredSigners;
    bool includeNetworkId;
    bool includeCollateralOutput;
    bool includeTotalCollateral;
    uint16_t numReferenceInputs;
    uint16_t numVoters;
    bool includeTreasury;
    bool includeDonation;
    uint16_t numWitnesses;
} init_apdu_params_t;

static inline uint8_t _item_flag(bool include) {
    return include ? ITEM_INCLUDED_YES : ITEM_INCLUDED_NO;
}

static inline void _append_u8(uint8_t* buffer, size_t* pos, uint8_t value) {
    buffer[(*pos)++] = value;
}

static inline void _append_u16_be(uint8_t* buffer, size_t* pos, uint16_t value) {
    buffer[(*pos)++] = (uint8_t)((value >> 8) & 0xFF);
    buffer[(*pos)++] = (uint8_t)(value & 0xFF);
}

static inline void _append_u32_be(uint8_t* buffer, size_t* pos, uint32_t value) {
    buffer[(*pos)++] = (uint8_t)((value >> 24) & 0xFF);
    buffer[(*pos)++] = (uint8_t)((value >> 16) & 0xFF);
    buffer[(*pos)++] = (uint8_t)((value >> 8) & 0xFF);
    buffer[(*pos)++] = (uint8_t)(value & 0xFF);
}

static inline void _append_u64_be(uint8_t* buffer, size_t* pos, uint64_t value) {
    for (int i = 7; i >= 0; i--) {
        buffer[(*pos)++] = (uint8_t)((value >> (i * 8)) & 0xFF);
    }
}

static inline size_t build_init_apdu(const init_apdu_params_t* params,
                                     uint8_t* out,
                                     size_t out_size) {
    size_t pos = 0;

    _append_u64_be(out, &pos, params->options);
    _append_u8(out, &pos, params->networkId);
    _append_u32_be(out, &pos, params->protocolMagic);
    _append_u8(out, &pos, params->signingMode);

    _append_u16_be(out, &pos, params->numInputs);
    _append_u16_be(out, &pos, params->numOutputs);

    _append_u8(out, &pos, _item_flag(params->includeTtl));
    _append_u16_be(out, &pos, params->numCertificates);
    _append_u16_be(out, &pos, params->numWithdrawals);

    _append_u8(out, &pos, _item_flag(params->includeAuxData));
    if (params->includeAuxData) {
        _append_u8(out, &pos, params->auxDataType);
        if (params->auxDataType == AUX_DATA_TYPE_ARBITRARY_HASH) {
            if (params->auxDataHash == NULL ||
                params->auxDataHashLen != AUX_DATA_HASH_LENGTH) {
                return 0;
            }
            memcpy(out + pos, params->auxDataHash, params->auxDataHashLen);
            pos += params->auxDataHashLen;
        }
    }

    _append_u8(out, &pos, _item_flag(params->includeValidityIntervalStart));
    _append_u16_be(out, &pos, params->numMintAssetGroups);
    _append_u8(out, &pos, _item_flag(params->includeScriptDataHash));
    _append_u16_be(out, &pos, params->numCollateralInputs);
    _append_u16_be(out, &pos, params->numRequiredSigners);
    _append_u8(out, &pos, _item_flag(params->includeNetworkId));
    _append_u8(out, &pos, _item_flag(params->includeCollateralOutput));
    _append_u8(out, &pos, _item_flag(params->includeTotalCollateral));
    _append_u16_be(out, &pos, params->numReferenceInputs);
    _append_u16_be(out, &pos, params->numVoters);
    _append_u8(out, &pos, _item_flag(params->includeTreasury));
    _append_u8(out, &pos, _item_flag(params->includeDonation));
    _append_u16_be(out, &pos, params->numWitnesses);

    (void) out_size;
    return pos;
}
