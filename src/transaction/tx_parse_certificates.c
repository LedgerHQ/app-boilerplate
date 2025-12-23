/*****************************************************************************
 *   Ledger App Cardano.
 *   (c) 2025 Vacuumlabs
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

#include "buffer.h"
#include "memory/mem.h"

#include "os.h"

#include "cardano_swo.h"
#include "utils/assert.h"
#include "utils/buffer_utils.h"
#include "utils/utils.h"
#include "tx_parse_certificates.h"
#include "transaction/tx.h"

/// Wire format for credential type encoding by Python client:
/// 0x22 = KEY_PATH (not directly representable in CBOR)
/// 0x33 = KEY_HASH (CBOR 0)
/// 0x55 = SCRIPT_HASH (CBOR 1)
static parser_status_e _parse_credential_type(buffer_t *buf, ext_credential_type_t *cred_type) {
    uint8_t cred_type_wire;
    if (!buffer_read_u8(buf, &cred_type_wire)) {
        return CERTIFICATES_PARSING_ERROR;
    }

    TRACE("Parsing credential type wire=0x%02x", cred_type_wire);

    switch (cred_type_wire) {
        case 0x22:
            *cred_type = EXT_CREDENTIAL_KEY_PATH;
            break;
        case 0x33:
            *cred_type = EXT_CREDENTIAL_KEY_HASH;
            break;
        case 0x55:
            *cred_type = EXT_CREDENTIAL_SCRIPT_HASH;
            break;
        default:
            return CERTIFICATES_PARSING_ERROR;
    }
    return PARSING_OK;
}

/// Parse credential data based on its type
static parser_status_e _parse_credential_data(buffer_t *buf,
                                              ext_credential_type_t cred_type,
                                              ext_credential_t *credential) {
    switch (cred_type) {
        case EXT_CREDENTIAL_KEY_PATH:
            if (!buffer_read_bip44_path(buf, &credential->keyPath)) {
                return CERTIFICATES_PARSING_ERROR;
            }
            break;
        case EXT_CREDENTIAL_KEY_HASH: {
            STATIC_ASSERT(SIZEOF(credential->keyHash) == ADDRESS_KEY_HASH_LENGTH,
                          "credential key hash size mismatch");
            if (!buffer_read_bytes(buf, credential->keyHash, ADDRESS_KEY_HASH_LENGTH)) {
                return CERTIFICATES_PARSING_ERROR;
            }
            break;
        }
        case EXT_CREDENTIAL_SCRIPT_HASH: {
            STATIC_ASSERT(SIZEOF(credential->scriptHash) == SCRIPT_HASH_LENGTH,
                          "credential script hash size mismatch");
            if (!buffer_read_bytes(buf, credential->scriptHash, SCRIPT_HASH_LENGTH)) {
                return CERTIFICATES_PARSING_ERROR;
            }
            break;
        }
        default:
            return CERTIFICATES_PARSING_ERROR;
    }
    return PARSING_OK;
}

/// Parse a complete stake credential (type + data)
parser_status_e parse_stake_credential(buffer_t *buf, ext_credential_t *credential) {
    ext_credential_type_t cred_type;
    parser_status_e status = _parse_credential_type(buf, &cred_type);
    if (status != PARSING_OK) {
        return status;
    }
    credential->type = cred_type;

    return _parse_credential_data(buf, cred_type, credential);
}

/// Parse CERTIFICATE_STAKE_REGISTRATION or CERTIFICATE_STAKE_DEREGISTRATION
parser_status_e parse_certificate_stake_registration_deregistration(
    buffer_t *buf,
    certificate_type_t cert_type,
    certificate_data_t *cert_data) {
    LEDGER_ASSERT(cert_type == CERTIFICATE_STAKE_REGISTRATION ||
                  cert_type == CERTIFICATE_STAKE_DEREGISTRATION,
                  "Invalid certificate type for stake registration/deregistration");

    cert_data->type = cert_type;
    return parse_stake_credential(buf, &cert_data->stakeCredential);
}

/// Parse CERTIFICATE_STAKE_DELEGATION
parser_status_e parse_certificate_stake_delegation(buffer_t *buf,
                                                  certificate_data_t *cert_data) {
    cert_data->type = CERTIFICATE_STAKE_DELEGATION;

    parser_status_e status = parse_stake_credential(buf, &cert_data->stakeCredential);
    if (status != PARSING_OK) {
        return status;
    }

    // Store pointer to pool key hash in raw buffer instead of copying
    uint8_t *hash_ptr = NULL;
    if (!buffer_read_bytes_ptr(buf, &hash_ptr, POOL_KEY_HASH_LENGTH)) {
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(hash_ptr != NULL);
    cert_data->poolKeyHash = hash_ptr;
    return PARSING_OK;
}

/// Parse CERTIFICATE_STAKE_REGISTRATION_CONWAY or CERTIFICATE_STAKE_DEREGISTRATION_CONWAY
parser_status_e parse_certificate_stake_registration_deregistration_conway(
    buffer_t *buf,
    certificate_type_t cert_type,
    certificate_data_t *cert_data) {
    LEDGER_ASSERT(cert_type == CERTIFICATE_STAKE_REGISTRATION_CONWAY ||
                  cert_type == CERTIFICATE_STAKE_DEREGISTRATION_CONWAY,
                  "Invalid certificate type for Conway stake registration/deregistration");

    cert_data->type = cert_type;

    parser_status_e status = parse_stake_credential(buf, &cert_data->stakeCredential);
    if (status != PARSING_OK) {
        return status;
    }

    if (!buffer_read_u64(buf, &cert_data->deposit, BE)) {
        return CERTIFICATES_PARSING_ERROR;
    }
    return PARSING_OK;
}

/// Parse CERTIFICATE_STAKE_POOL_RETIREMENT
parser_status_e parse_certificate_stake_pool_retirement(buffer_t *buf,
                                                       certificate_data_t *cert_data) {
    cert_data->type = CERTIFICATE_STAKE_POOL_RETIREMENT;

    ext_credential_type_t pool_cred_type;
    parser_status_e status = _parse_credential_type(buf, &pool_cred_type);
    if (status != PARSING_OK) {
        return status;
    }

    cert_data->poolCredential.type = pool_cred_type;
    status = _parse_credential_data(buf, pool_cred_type, &cert_data->poolCredential);
    if (status != PARSING_OK) {
        return status;
    }

    if (!buffer_read_u64(buf, &cert_data->retirementEpoch, BE)) {
        return CERTIFICATES_PARSING_ERROR;
    }
    return PARSING_OK;
}

/// Helper to parse DRep (Delegated Representative) specification
static parser_status_e _parse_drep(buffer_t *buf, ext_drep_t *drep) {
    uint8_t drep_type_wire;
    if (!buffer_read_u8(buf, &drep_type_wire)) {
        return CERTIFICATES_PARSING_ERROR;
    }

    ext_drep_type_t drep_type;
    switch (drep_type_wire) {
        case 0x22:
            drep_type = EXT_DREP_KEY_PATH;
            break;
        case 0x33:
            drep_type = EXT_DREP_KEY_HASH;
            break;
        case 0x55:
            drep_type = EXT_DREP_SCRIPT_HASH;
            break;
        case 0x02:
            drep_type = EXT_DREP_ABSTAIN;
            break;
        case 0x03:
            drep_type = EXT_DREP_NO_CONFIDENCE;
            break;
        default:
            return CERTIFICATES_PARSING_ERROR;
    }
    drep->type = drep_type;

    switch (drep_type) {
        case EXT_DREP_KEY_PATH:
            if (!buffer_read_bip44_path(buf, &drep->keyPath)) {
                return CERTIFICATES_PARSING_ERROR;
            }
            break;
        case EXT_DREP_KEY_HASH: {
            STATIC_ASSERT(SIZEOF(drep->keyHash) == ADDRESS_KEY_HASH_LENGTH,
                          "drep key hash size mismatch");
            if (!buffer_read_bytes(buf, drep->keyHash, ADDRESS_KEY_HASH_LENGTH)) {
                return CERTIFICATES_PARSING_ERROR;
            }
            break;
        }
        case EXT_DREP_SCRIPT_HASH: {
            STATIC_ASSERT(SIZEOF(drep->scriptHash) == SCRIPT_HASH_LENGTH,
                          "drep script hash size mismatch");
            if (!buffer_read_bytes(buf, drep->scriptHash, SCRIPT_HASH_LENGTH)) {
                return CERTIFICATES_PARSING_ERROR;
            }
            break;
        }
        case EXT_DREP_ABSTAIN:
        case EXT_DREP_NO_CONFIDENCE:
            // These types have no additional data
            break;
        default:
            return CERTIFICATES_PARSING_ERROR;
    }
    return PARSING_OK;
}

/// Helper to parse anchor (URL + hash)
static parser_status_e _parse_anchor(buffer_t *buf, anchor_t *anchor) {
    // Check if anchor is present (1 byte flag)
    uint8_t anchor_present;
    if (!buffer_read_u8(buf, &anchor_present)) {
        return CERTIFICATES_PARSING_ERROR;
    }

    if (anchor_present == 0) {
        anchor->isIncluded = false;
        return PARSING_OK;
    }

    anchor->isIncluded = true;

    // Read URL length
    uint8_t url_len_byte;
    if (!buffer_read_u8(buf, &url_len_byte)) {
        return CERTIFICATES_PARSING_ERROR;
    }
    anchor->urlLength = url_len_byte;

    if (anchor->urlLength > ANCHOR_URL_LENGTH_MAX) {
        return CERTIFICATES_PARSING_ERROR;
    }

    // Store pointer to URL in raw buffer instead of copying
    // Note: urlLength can be 0 for empty URLs, which is valid
    uint8_t *url_ptr = NULL;
    if (!buffer_read_bytes_ptr(buf, &url_ptr, anchor->urlLength)) {
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(url_ptr != NULL);
    anchor->url = url_ptr;

    // Store pointer to hash in raw buffer instead of copying
    uint8_t *hash_ptr = NULL;
    if (!buffer_read_bytes_ptr(buf, &hash_ptr, ANCHOR_HASH_LENGTH)) {
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(hash_ptr != NULL);
    anchor->hash = hash_ptr;

    return PARSING_OK;
}

/// Parse CERTIFICATE_VOTE_DELEGATION
parser_status_e parse_certificate_vote_delegation(buffer_t *buf,
                                                 certificate_data_t *cert_data) {
    cert_data->type = CERTIFICATE_VOTE_DELEGATION;

    parser_status_e status = parse_stake_credential(buf, &cert_data->stakeCredential);
    if (status != PARSING_OK) {
        return status;
    }

    return _parse_drep(buf, &cert_data->drep);
}

/// Parse CERTIFICATE_AUTHORIZE_COMMITTEE_HOT
parser_status_e parse_certificate_authorize_committee_hot(buffer_t *buf,
                                                         certificate_data_t *cert_data) {
    cert_data->type = CERTIFICATE_AUTHORIZE_COMMITTEE_HOT;

    parser_status_e status = parse_stake_credential(buf, &cert_data->coldCredential);
    if (status != PARSING_OK) {
        return status;
    }

    // Second credential is the hot credential
    status = _parse_credential_type(buf, &cert_data->hotCredential.type);
    if (status != PARSING_OK) {
        return status;
    }

    return _parse_credential_data(buf, cert_data->hotCredential.type, &cert_data->hotCredential);
}

/// Parse CERTIFICATE_RESIGN_COMMITTEE_COLD
parser_status_e parse_certificate_resign_committee_cold(buffer_t *buf,
                                                       certificate_data_t *cert_data) {
    cert_data->type = CERTIFICATE_RESIGN_COMMITTEE_COLD;

    parser_status_e status = parse_stake_credential(buf, &cert_data->coldCredential);
    if (status != PARSING_OK) {
        return status;
    }

    return _parse_anchor(buf, &cert_data->anchor);
}

/// Parse CERTIFICATE_DREP_REGISTRATION
parser_status_e parse_certificate_drep_registration(buffer_t *buf,
                                                   certificate_data_t *cert_data) {
    cert_data->type = CERTIFICATE_DREP_REGISTRATION;

    parser_status_e status = parse_stake_credential(buf, &cert_data->dRepCredential);
    if (status != PARSING_OK) {
        return status;
    }

    if (!buffer_read_u64(buf, &cert_data->deposit, BE)) {
        return CERTIFICATES_PARSING_ERROR;
    }

    return _parse_anchor(buf, &cert_data->anchor);
}

/// Parse CERTIFICATE_DREP_DEREGISTRATION
parser_status_e parse_certificate_drep_deregistration(buffer_t *buf,
                                                     certificate_data_t *cert_data) {
    cert_data->type = CERTIFICATE_DREP_DEREGISTRATION;

    parser_status_e status = parse_stake_credential(buf, &cert_data->dRepCredential);
    if (status != PARSING_OK) {
        return status;
    }

    if (!buffer_read_u64(buf, &cert_data->deposit, BE)) {
        return CERTIFICATES_PARSING_ERROR;
    }

    return PARSING_OK;
}

/// Parse CERTIFICATE_DREP_UPDATE
parser_status_e parse_certificate_drep_update(buffer_t *buf,
                                             certificate_data_t *cert_data) {
    cert_data->type = CERTIFICATE_DREP_UPDATE;

    parser_status_e status = parse_stake_credential(buf, &cert_data->dRepCredential);
    if (status != PARSING_OK) {
        return status;
    }

    return _parse_anchor(buf, &cert_data->anchor);
}
