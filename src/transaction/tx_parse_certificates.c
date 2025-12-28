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
/// 0x00 = KEY_HASH
/// 0x01 = SCRIPT_HASH
/// 0x02 = KEY_PATH (converted to KEY_HASH before CBOR serialization)
/// CBOR credential types (tx_hash_builder, per CDDL): 0=KEY_HASH, 1=SCRIPT_HASH
static parser_status_e _parse_credential_type(buffer_t *buf, ext_credential_type_t *cred_type) {
    uint8_t cred_type_wire;
    if (!buffer_read_u8(buf, &cred_type_wire)) {
        TRACE("Failed to read credential type byte");
        return CERTIFICATES_PARSING_ERROR;
    }

    TRACE("Parsing credential type wire=0x%02x", cred_type_wire);

    switch (cred_type_wire) {
        case 0x00:  // KEY_HASH
            *cred_type = EXT_CREDENTIAL_KEY_HASH;
            TRACE("Credential type: KEY_HASH");
            break;
        case 0x01:  // SCRIPT_HASH
            *cred_type = EXT_CREDENTIAL_SCRIPT_HASH;
            TRACE("Credential type: SCRIPT_HASH");
            break;
        case 0x02:  // KEY_PATH
            *cred_type = EXT_CREDENTIAL_KEY_PATH;
            TRACE("Credential type: KEY_PATH");
            break;
        default:
            TRACE("Invalid credential type wire value: 0x%02x", cred_type_wire);
            return CERTIFICATES_PARSING_ERROR;
    }
    return PARSING_OK;
}

/// Parse credential data based on its type
static parser_status_e _parse_credential_data(buffer_t *buf,
                                              ext_credential_type_t cred_type,
                                              ext_credential_t *credential) {
    TRACE("Parsing credential data for type=%u", cred_type);
    switch (cred_type) {
        case EXT_CREDENTIAL_KEY_PATH:
            if (!buffer_read_bip44_path(buf, &credential->keyPath)) {
                TRACE("Failed to read BIP44 path");
                return CERTIFICATES_PARSING_ERROR;
            }
            TRACE("Successfully parsed KEY_PATH credential");
            break;
        case EXT_CREDENTIAL_KEY_HASH: {
            STATIC_ASSERT(SIZEOF(credential->keyHash) == ADDRESS_KEY_HASH_LENGTH,
                          "credential key hash size mismatch");
            if (!buffer_read_bytes(buf, credential->keyHash, ADDRESS_KEY_HASH_LENGTH)) {
                TRACE("Failed to read key hash");
                return CERTIFICATES_PARSING_ERROR;
            }
            TRACE("Successfully parsed KEY_HASH credential");
            break;
        }
        case EXT_CREDENTIAL_SCRIPT_HASH: {
            STATIC_ASSERT(SIZEOF(credential->scriptHash) == SCRIPT_HASH_LENGTH,
                          "credential script hash size mismatch");
            if (!buffer_read_bytes(buf, credential->scriptHash, SCRIPT_HASH_LENGTH)) {
                TRACE("Failed to read script hash");
                return CERTIFICATES_PARSING_ERROR;
            }
            TRACE("Successfully parsed SCRIPT_HASH credential");
            break;
        }
        default:
            TRACE("Invalid credential type: %u", cred_type);
            return CERTIFICATES_PARSING_ERROR;
    }
    return PARSING_OK;
}

/// Parse a complete stake credential (type + data)
parser_status_e parse_stake_credential(buffer_t *buf, ext_credential_t *credential) {
    TRACE("Parsing stake credential");
    ext_credential_type_t cred_type;
    parser_status_e status = _parse_credential_type(buf, &cred_type);
    if (status != PARSING_OK) {
        TRACE("Failed to parse credential type");
        return status;
    }
    credential->type = cred_type;

    status = _parse_credential_data(buf, cred_type, credential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse credential data");
        return status;
    }
    TRACE("Successfully parsed stake credential");
    return PARSING_OK;
}

/// Parse CERTIFICATE_STAKE_REGISTRATION or CERTIFICATE_STAKE_DEREGISTRATION
parser_status_e parse_certificate_stake_registration_deregistration(
    buffer_t *buf,
    certificate_type_t cert_type,
    certificate_data_t *cert_data) {
    TRACE("Parsing STAKE_REGISTRATION/DEREGISTRATION certificate, type=%u", cert_type);
    LEDGER_ASSERT(cert_type == CERTIFICATE_STAKE_REGISTRATION ||
                  cert_type == CERTIFICATE_STAKE_DEREGISTRATION,
                  "Invalid certificate type for stake registration/deregistration");

    cert_data->type = cert_type;
    parser_status_e status = parse_stake_credential(buf, &cert_data->stakeCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse stake credential");
        return status;
    }
    TRACE("Successfully parsed STAKE_REGISTRATION/DEREGISTRATION");
    return PARSING_OK;
}

/// Parse CERTIFICATE_STAKE_DELEGATION
parser_status_e parse_certificate_stake_delegation(buffer_t *buf,
                                                  certificate_data_t *cert_data) {
    TRACE("Parsing STAKE_DELEGATION certificate");
    cert_data->type = CERTIFICATE_STAKE_DELEGATION;

    parser_status_e status = parse_stake_credential(buf, &cert_data->stakeCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse stake credential");
        return status;
    }

    // Store pointer to pool key hash in raw buffer instead of copying
    uint8_t *hash_ptr = NULL;
    if (!buffer_read_bytes_ptr(buf, &hash_ptr, POOL_KEY_HASH_LENGTH)) {
        TRACE("Failed to read pool key hash");
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(hash_ptr != NULL);
    cert_data->poolKeyHash = hash_ptr;
    TRACE("Successfully parsed STAKE_DELEGATION");
    return PARSING_OK;
}

/// Parse CERTIFICATE_STAKE_REGISTRATION_CONWAY or CERTIFICATE_STAKE_DEREGISTRATION_CONWAY
parser_status_e parse_certificate_stake_registration_deregistration_conway(
    buffer_t *buf,
    certificate_type_t cert_type,
    certificate_data_t *cert_data) {
    TRACE("Parsing STAKE_REGISTRATION/DEREGISTRATION_CONWAY certificate, type=%u", cert_type);
    LEDGER_ASSERT(cert_type == CERTIFICATE_STAKE_REGISTRATION_CONWAY ||
                  cert_type == CERTIFICATE_STAKE_DEREGISTRATION_CONWAY,
                  "Invalid certificate type for Conway stake registration/deregistration");

    cert_data->type = cert_type;

    parser_status_e status = parse_stake_credential(buf, &cert_data->stakeCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse stake credential");
        return status;
    }

    if (!buffer_read_u64(buf, &cert_data->deposit, BE)) {
        TRACE("Failed to parse deposit");
        return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Successfully parsed STAKE_REGISTRATION/DEREGISTRATION_CONWAY, deposit=%llu", cert_data->deposit);
    return PARSING_OK;
}

/// Parse CERTIFICATE_STAKE_POOL_RETIREMENT
parser_status_e parse_certificate_stake_pool_retirement(buffer_t *buf,
                                                       certificate_data_t *cert_data) {
    TRACE("Parsing STAKE_POOL_RETIREMENT certificate, buf->offset=%u buf->size=%u", buf->offset, buf->size);
    cert_data->type = CERTIFICATE_STAKE_POOL_RETIREMENT;

    ext_credential_type_t pool_cred_type;
    TRACE("About to parse pool credential type at offset=%u", buf->offset);
    parser_status_e status = _parse_credential_type(buf, &pool_cred_type);
    if (status != PARSING_OK) {
        TRACE("Failed to parse pool credential type, status=%d", status);
        return status;
    }

    cert_data->poolCredential.type = pool_cred_type;
    status = _parse_credential_data(buf, pool_cred_type, &cert_data->poolCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse pool credential data");
        return status;
    }

    if (!buffer_read_u64(buf, &cert_data->retirementEpoch, BE)) {
        TRACE("Failed to parse retirement epoch");
        return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Successfully parsed pool retirement, epoch=%llu", cert_data->retirementEpoch);
    return PARSING_OK;
}

/// Helper to parse DRep (Delegated Representative) specification
/// Python client encoding:
/// 0x00 = KEY_HASH
/// 0x01 = SCRIPT_HASH
/// 0x02 = ABSTAIN
/// 0x03 = NO_CONFIDENCE
/// 0x64 (100) = KEY_PATH
static parser_status_e _parse_drep(buffer_t *buf, ext_drep_t *drep) {
    TRACE("Parsing DRep");
    uint8_t drep_type_wire;
    if (!buffer_read_u8(buf, &drep_type_wire)) {
        TRACE("Failed to read DRep type byte");
        return CERTIFICATES_PARSING_ERROR;
    }

    TRACE("DRep type wire=0x%02x", drep_type_wire);
    ext_drep_type_t drep_type;
    switch (drep_type_wire) {
        case 0x00:  // KEY_HASH
            drep_type = EXT_DREP_KEY_HASH;
            TRACE("DRep type: KEY_HASH");
            break;
        case 0x01:  // SCRIPT_HASH
            drep_type = EXT_DREP_SCRIPT_HASH;
            TRACE("DRep type: SCRIPT_HASH");
            break;
        case 0x02:  // ABSTAIN
            drep_type = EXT_DREP_ABSTAIN;
            TRACE("DRep type: ABSTAIN");
            break;
        case 0x03:  // NO_CONFIDENCE
            drep_type = EXT_DREP_NO_CONFIDENCE;
            TRACE("DRep type: NO_CONFIDENCE");
            break;
        case 0x64:  // KEY_PATH (100)
            drep_type = EXT_DREP_KEY_PATH;
            TRACE("DRep type: KEY_PATH");
            break;
        default:
            TRACE("Invalid DRep type wire: 0x%02x", drep_type_wire);
            return CERTIFICATES_PARSING_ERROR;
    }
    drep->type = drep_type;

    switch (drep_type) {
        case EXT_DREP_KEY_PATH:
            if (!buffer_read_bip44_path(buf, &drep->keyPath)) {
                TRACE("Failed to read DRep BIP44 path");
                return CERTIFICATES_PARSING_ERROR;
            }
            TRACE("Successfully parsed DRep KEY_PATH");
            break;
        case EXT_DREP_KEY_HASH: {
            STATIC_ASSERT(SIZEOF(drep->keyHash) == ADDRESS_KEY_HASH_LENGTH,
                          "drep key hash size mismatch");
            if (!buffer_read_bytes(buf, drep->keyHash, ADDRESS_KEY_HASH_LENGTH)) {
                TRACE("Failed to read DRep key hash");
                return CERTIFICATES_PARSING_ERROR;
            }
            TRACE("Successfully parsed DRep KEY_HASH");
            break;
        }
        case EXT_DREP_SCRIPT_HASH: {
            STATIC_ASSERT(SIZEOF(drep->scriptHash) == SCRIPT_HASH_LENGTH,
                          "drep script hash size mismatch");
            if (!buffer_read_bytes(buf, drep->scriptHash, SCRIPT_HASH_LENGTH)) {
                TRACE("Failed to read DRep script hash");
                return CERTIFICATES_PARSING_ERROR;
            }
            TRACE("Successfully parsed DRep SCRIPT_HASH");
            break;
        }
        case EXT_DREP_ABSTAIN:
        case EXT_DREP_NO_CONFIDENCE:
            TRACE("DRep has no additional data");
            break;
        default:
            TRACE("Invalid DRep type: %u", drep_type);
            return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Successfully parsed DRep");
    return PARSING_OK;
}

/// Helper to parse anchor (URL + hash)
static parser_status_e _parse_anchor(buffer_t *buf, anchor_t *anchor) {
    TRACE("Parsing anchor");
    // Check if anchor is present (1 byte flag)
    uint8_t anchor_present;
    if (!buffer_read_u8(buf, &anchor_present)) {
        TRACE("Failed to read anchor present flag");
        return CERTIFICATES_PARSING_ERROR;
    }

    TRACE("Anchor present flag: %u", anchor_present);
    if (anchor_present == 0) {
        anchor->isIncluded = false;
        TRACE("Anchor not included");
        return PARSING_OK;
    }

    anchor->isIncluded = true;

    // Read URL length
    uint8_t url_len_byte;
    if (!buffer_read_u8(buf, &url_len_byte)) {
        TRACE("Failed to read anchor URL length");
        return CERTIFICATES_PARSING_ERROR;
    }
    anchor->urlLength = url_len_byte;
    TRACE("Anchor URL length: %u", anchor->urlLength);

    if (anchor->urlLength > ANCHOR_URL_LENGTH_MAX) {
        TRACE("Anchor URL length exceeds maximum: %u > %u", anchor->urlLength, ANCHOR_URL_LENGTH_MAX);
        return CERTIFICATES_PARSING_ERROR;
    }

    // Store pointer to URL in raw buffer instead of copying
    // Note: urlLength can be 0 for empty URLs, which is valid
    uint8_t *url_ptr = NULL;
    if (!buffer_read_bytes_ptr(buf, &url_ptr, anchor->urlLength)) {
        TRACE("Failed to read anchor URL");
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(url_ptr != NULL);
    anchor->url = url_ptr;
    TRACE("Successfully parsed anchor URL");

    // Store pointer to hash in raw buffer instead of copying
    uint8_t *hash_ptr = NULL;
    if (!buffer_read_bytes_ptr(buf, &hash_ptr, ANCHOR_HASH_LENGTH)) {
        TRACE("Failed to read anchor hash");
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(hash_ptr != NULL);
    anchor->hash = hash_ptr;

    TRACE("Successfully parsed anchor");
    return PARSING_OK;
}

/// Parse CERTIFICATE_VOTE_DELEGATION
parser_status_e parse_certificate_vote_delegation(buffer_t *buf,
                                                 certificate_data_t *cert_data) {
    TRACE("Parsing VOTE_DELEGATION certificate");
    cert_data->type = CERTIFICATE_VOTE_DELEGATION;

    parser_status_e status = parse_stake_credential(buf, &cert_data->stakeCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse stake credential");
        return status;
    }

    status = _parse_drep(buf, &cert_data->drep);
    if (status != PARSING_OK) {
        TRACE("Failed to parse DRep");
        return status;
    }
    TRACE("Successfully parsed VOTE_DELEGATION");
    return PARSING_OK;
}

/// Parse CERTIFICATE_AUTHORIZE_COMMITTEE_HOT
parser_status_e parse_certificate_authorize_committee_hot(buffer_t *buf,
                                                         certificate_data_t *cert_data) {
    TRACE("Parsing AUTHORIZE_COMMITTEE_HOT certificate");
    cert_data->type = CERTIFICATE_AUTHORIZE_COMMITTEE_HOT;

    parser_status_e status = parse_stake_credential(buf, &cert_data->coldCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse cold credential");
        return status;
    }

    // Second credential is the hot credential
    TRACE("Parsing hot credential");
    status = _parse_credential_type(buf, &cert_data->hotCredential.type);
    if (status != PARSING_OK) {
        TRACE("Failed to parse hot credential type");
        return status;
    }

    status = _parse_credential_data(buf, cert_data->hotCredential.type, &cert_data->hotCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse hot credential data");
        return status;
    }
    TRACE("Successfully parsed AUTHORIZE_COMMITTEE_HOT");
    return PARSING_OK;
}

/// Parse CERTIFICATE_RESIGN_COMMITTEE_COLD
parser_status_e parse_certificate_resign_committee_cold(buffer_t *buf,
                                                       certificate_data_t *cert_data) {
    TRACE("Parsing RESIGN_COMMITTEE_COLD certificate");
    cert_data->type = CERTIFICATE_RESIGN_COMMITTEE_COLD;

    parser_status_e status = parse_stake_credential(buf, &cert_data->coldCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse cold credential");
        return status;
    }

    status = _parse_anchor(buf, &cert_data->anchor);
    if (status != PARSING_OK) {
        TRACE("Failed to parse anchor");
        return status;
    }
    TRACE("Successfully parsed RESIGN_COMMITTEE_COLD");
    return PARSING_OK;
}

/// Parse CERTIFICATE_DREP_REGISTRATION
parser_status_e parse_certificate_drep_registration(buffer_t *buf,
                                                   certificate_data_t *cert_data) {
    TRACE("Parsing DREP_REGISTRATION certificate");
    cert_data->type = CERTIFICATE_DREP_REGISTRATION;

    parser_status_e status = parse_stake_credential(buf, &cert_data->dRepCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse DRep credential");
        return status;
    }

    if (!buffer_read_u64(buf, &cert_data->deposit, BE)) {
        TRACE("Failed to parse deposit");
        return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Deposit: %llu", cert_data->deposit);

    status = _parse_anchor(buf, &cert_data->anchor);
    if (status != PARSING_OK) {
        TRACE("Failed to parse anchor");
        return status;
    }
    TRACE("Successfully parsed DREP_REGISTRATION");
    return PARSING_OK;
}

/// Parse CERTIFICATE_DREP_DEREGISTRATION
parser_status_e parse_certificate_drep_deregistration(buffer_t *buf,
                                                     certificate_data_t *cert_data) {
    TRACE("Parsing DREP_DEREGISTRATION certificate");
    cert_data->type = CERTIFICATE_DREP_DEREGISTRATION;

    parser_status_e status = parse_stake_credential(buf, &cert_data->dRepCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse DRep credential");
        return status;
    }

    if (!buffer_read_u64(buf, &cert_data->deposit, BE)) {
        TRACE("Failed to parse deposit");
        return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Deposit: %llu", cert_data->deposit);

    TRACE("Successfully parsed DREP_DEREGISTRATION");
    return PARSING_OK;
}

/// Parse CERTIFICATE_DREP_UPDATE
parser_status_e parse_certificate_drep_update(buffer_t *buf,
                                             certificate_data_t *cert_data) {
    TRACE("Parsing DREP_UPDATE certificate");
    cert_data->type = CERTIFICATE_DREP_UPDATE;

    parser_status_e status = parse_stake_credential(buf, &cert_data->dRepCredential);
    if (status != PARSING_OK) {
        TRACE("Failed to parse DRep credential");
        return status;
    }

    status = _parse_anchor(buf, &cert_data->anchor);
    if (status != PARSING_OK) {
        TRACE("Failed to parse anchor");
        return status;
    }
    TRACE("Successfully parsed DREP_UPDATE");
    return PARSING_OK;
}
