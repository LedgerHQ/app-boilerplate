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
#include "memory/flist.h"

#include "os.h"

#include "cardano_swo.h"
#include "utils/assert.h"
#include "utils/buffer_utils.h"
#include "utils/utils.h"
#include "utils/textUtils.h"
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
    ext_credential_type_t cred_type = {0};
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
    if (!buffer_read_bytes_ptr(buf, &cert_data->poolKeyHash, POOL_KEY_HASH_LENGTH)) {
        TRACE("Failed to read pool key hash");
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(cert_data->poolKeyHash != NULL);
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
    TRACE("Successfully parsed STAKE_REGISTRATION/DEREGISTRATION_CONWAY, deposit=");
    TRACE_UINT64(cert_data->deposit);
    return PARSING_OK;
}

/// Parse CERTIFICATE_STAKE_POOL_RETIREMENT
parser_status_e parse_certificate_stake_pool_retirement(buffer_t *buf,
                                                       certificate_data_t *cert_data) {
    TRACE("Parsing STAKE_POOL_RETIREMENT certificate, buf->offset=%u buf->size=%u", buf->offset, buf->size);
    cert_data->type = CERTIFICATE_STAKE_POOL_RETIREMENT;

    ext_credential_type_t pool_cred_type = {0};
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
    TRACE("Successfully parsed pool retirement, epoch=");
    TRACE_UINT64(cert_data->retirementEpoch);
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
    ext_drep_type_t drep_type = {0};
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

    if (anchor->urlLength > MAX_ANCHOR_URL_LENGTH) {
        TRACE("Anchor URL length exceeds maximum: %u > %u", anchor->urlLength, MAX_ANCHOR_URL_LENGTH);
        return CERTIFICATES_PARSING_ERROR;
    }

    // Store pointer to URL in raw buffer instead of copying
    // Note: urlLength can be 0 for empty URLs, which is valid
    if (!buffer_read_bytes_ptr(buf, &anchor->url, anchor->urlLength)) {
        TRACE("Failed to read anchor URL");
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(anchor->url != NULL);
    if (!str_isPrintableAsciiWithoutSpaces(anchor->url, anchor->urlLength)) {
        TRACE("Anchor URL contains non-printable ASCII or spaces");
        return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Successfully parsed anchor URL");

    // Store pointer to hash in raw buffer instead of copying
    if (!buffer_read_bytes_ptr(buf, &anchor->hash, ANCHOR_HASH_LENGTH)) {
        TRACE("Failed to read anchor hash");
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(anchor->hash != NULL);

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
    TRACE("Deposit: ");
    TRACE_UINT64(cert_data->deposit);

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
    TRACE("Deposit: ");
    TRACE_UINT64(cert_data->deposit);

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

/// Helper to parse pool ID (operator key - hash or path)
static parser_status_e _parse_pool_id(buffer_t *buf, pool_id_t *pool_id) {
    TRACE("Parsing pool ID");
    uint8_t pool_id_type_wire;
    if (!buffer_read_u8(buf, &pool_id_type_wire)) {
        TRACE("Failed to read pool ID type byte");
        return CERTIFICATES_PARSING_ERROR;
    }

    TRACE("Pool ID type wire=0x%02x", pool_id_type_wire);
    switch (pool_id_type_wire) {
        case 0x00:  // KEY_HASH (pool key hash)
            pool_id->keyReferenceType = KEY_REFERENCE_HASH;
            if (!buffer_read_bytes_ptr(buf, &pool_id->hash, POOL_KEY_HASH_LENGTH)) {
                TRACE("Failed to read pool key hash");
                return CERTIFICATES_PARSING_ERROR;
            }
            ASSERT(pool_id->hash != NULL);
            TRACE("Successfully parsed pool ID as KEY_HASH");
            break;
        case 0x02:  // KEY_PATH (pool cold key path)
            pool_id->keyReferenceType = KEY_REFERENCE_PATH;
            if (!buffer_read_bip44_path(buf, &pool_id->path)) {
                TRACE("Failed to read pool key path");
                return CERTIFICATES_PARSING_ERROR;
            }
            TRACE("Successfully parsed pool ID as KEY_PATH");
            break;
        default:
            TRACE("Invalid pool ID type wire value: 0x%02x", pool_id_type_wire);
            return CERTIFICATES_PARSING_ERROR;
    }
    return PARSING_OK;
}

/// Helper to parse a single pool relay
static parser_status_e _parse_pool_relay(buffer_t *buf, pool_relay_t *relay) {
    TRACE("Parsing pool relay");
    uint8_t relay_type;
    if (!buffer_read_u8(buf, &relay_type)) {
        TRACE("Failed to read relay type");
        return CERTIFICATES_PARSING_ERROR;
    }

    TRACE("Relay type: %u", relay_type);
    switch (relay_type) {
        case RELAY_SINGLE_HOST_IP: {
            // Format: type (0) + port + ipv4 (optional) + ipv6 (optional)
            relay->format = RELAY_SINGLE_HOST_IP;

            // Port (2 bytes) - null or value
            uint8_t port_present;
            bool port_included = false;
            if (!buffer_read_u8(buf, &port_present)) {
                TRACE("Failed to read port present flag");
                return CERTIFICATES_PARSING_ERROR;
            }
            if (!parseIncluded(port_present, &port_included)) {
                TRACE("Invalid port present flag: %u", port_present);
                return CERTIFICATES_PARSING_ERROR;
            }
            relay->port.isNull = !port_included;
            if (port_included) {
                if (!buffer_read_u16(buf, &relay->port.number, BE)) {
                    TRACE("Failed to read port number");
                    return CERTIFICATES_PARSING_ERROR;
                }
                TRACE("Relay port: %u", relay->port.number);
            }
            if (relay->port.isNull) {
                TRACE("Relay port missing");
                return CERTIFICATES_PARSING_ERROR;
            }

            // IPv4 (optional)
            uint8_t ipv4_present;
            bool ipv4_included = false;
            if (!buffer_read_u8(buf, &ipv4_present)) {
                TRACE("Failed to read IPv4 present flag");
                return CERTIFICATES_PARSING_ERROR;
            }
            if (!parseIncluded(ipv4_present, &ipv4_included)) {
                TRACE("Invalid IPv4 present flag: %u", ipv4_present);
                return CERTIFICATES_PARSING_ERROR;
            }
            relay->ipv4.isNull = !ipv4_included;
            if (ipv4_included) {
                if (!buffer_read_bytes_ptr(buf, &relay->ipv4.ip, IPV4_LENGTH)) {
                    TRACE("Failed to read IPv4 address");
                    return CERTIFICATES_PARSING_ERROR;
                }
                ASSERT(relay->ipv4.ip != NULL);
                TRACE("Relay IPv4 present");
            }

            // IPv6 (optional)
            uint8_t ipv6_present;
            bool ipv6_included = false;
            if (!buffer_read_u8(buf, &ipv6_present)) {
                TRACE("Failed to read IPv6 present flag");
                return CERTIFICATES_PARSING_ERROR;
            }
            if (!parseIncluded(ipv6_present, &ipv6_included)) {
                TRACE("Invalid IPv6 present flag: %u", ipv6_present);
                return CERTIFICATES_PARSING_ERROR;
            }
            relay->ipv6.isNull = !ipv6_included;
            if (ipv6_included) {
                if (!buffer_read_bytes_ptr(buf, &relay->ipv6.ip, IPV6_LENGTH)) {
                    TRACE("Failed to read IPv6 address");
                    return CERTIFICATES_PARSING_ERROR;
                }
                ASSERT(relay->ipv6.ip != NULL);
                TRACE("Relay IPv6 present");
            }
            if (relay->ipv4.isNull && relay->ipv6.isNull) {
                TRACE("Relay missing IPv4/IPv6 address");
                return CERTIFICATES_PARSING_ERROR;
            }
            break;
        }
        case RELAY_SINGLE_HOST_NAME: {
            // Format: type (1) + port + dns_name
            relay->format = RELAY_SINGLE_HOST_NAME;

            // Port (2 bytes) - null or value
            uint8_t port_present;
            bool port_included = false;
            if (!buffer_read_u8(buf, &port_present)) {
                TRACE("Failed to read port present flag");
                return CERTIFICATES_PARSING_ERROR;
            }
            if (!parseIncluded(port_present, &port_included)) {
                TRACE("Invalid port present flag: %u", port_present);
                return CERTIFICATES_PARSING_ERROR;
            }
            relay->port.isNull = !port_included;
            if (port_included) {
                if (!buffer_read_u16(buf, &relay->port.number, BE)) {
                    TRACE("Failed to read port number");
                    return CERTIFICATES_PARSING_ERROR;
                }
                TRACE("Relay port: %u", relay->port.number);
            }
            if (relay->port.isNull) {
                TRACE("Relay port missing");
                return CERTIFICATES_PARSING_ERROR;
            }

            // DNS name (length + data)
            uint8_t dns_len;
            if (!buffer_read_u8(buf, &dns_len)) {
                TRACE("Failed to read DNS name length");
                return CERTIFICATES_PARSING_ERROR;
            }
            relay->dnsNameSize = dns_len;

            if (dns_len > 0) {
                if (dns_len > MAX_DNS_NAME_LENGTH) {
                    TRACE("DNS name length exceeds maximum: %u > %u", dns_len, MAX_DNS_NAME_LENGTH);
                    return CERTIFICATES_PARSING_ERROR;
                }
                if (!buffer_read_bytes_ptr(buf, &relay->dnsName, dns_len)) {
                    TRACE("Failed to read DNS name");
                    return CERTIFICATES_PARSING_ERROR;
                }
                ASSERT(relay->dnsName != NULL);
            } else {
                relay->dnsName = NULL;
            }
            if (relay->dnsNameSize == 0) {
                TRACE("Relay DNS name missing");
                return CERTIFICATES_PARSING_ERROR;
            }
            if (!str_isUnambiguousAscii(relay->dnsName, relay->dnsNameSize)) {
                TRACE("Relay DNS name contains non-ASCII characters");
                return CERTIFICATES_PARSING_ERROR;
            }
            TRACE("Relay DNS name length: %u", dns_len);
            break;
        }
        case RELAY_MULTIPLE_HOST_NAME: {
            // Format: type (2) + dns_name (single SRV record)
            relay->format = RELAY_MULTIPLE_HOST_NAME;
            relay->port.isNull = true;
            relay->ipv4.isNull = true;
            relay->ipv6.isNull = true;

            // DNS name (length + data)
            uint8_t dns_len;
            if (!buffer_read_u8(buf, &dns_len)) {
                TRACE("Failed to read DNS name length");
                return CERTIFICATES_PARSING_ERROR;
            }
            relay->dnsNameSize = dns_len;

            if (dns_len > 0) {
                if (dns_len > MAX_DNS_NAME_LENGTH) {
                    TRACE("DNS name length exceeds maximum: %u > %u", dns_len, MAX_DNS_NAME_LENGTH);
                    return CERTIFICATES_PARSING_ERROR;
                }
                if (!buffer_read_bytes_ptr(buf, &relay->dnsName, dns_len)) {
                    TRACE("Failed to read DNS name");
                    return CERTIFICATES_PARSING_ERROR;
                }
                ASSERT(relay->dnsName != NULL);
            } else {
                relay->dnsName = NULL;
            }
            if (relay->dnsNameSize == 0) {
                TRACE("Relay DNS name missing");
                return CERTIFICATES_PARSING_ERROR;
            }
            if (!str_isUnambiguousAscii(relay->dnsName, relay->dnsNameSize)) {
                TRACE("Relay DNS name contains non-ASCII characters");
                return CERTIFICATES_PARSING_ERROR;
            }
            TRACE("Relay multi-host DNS name length: %u", dns_len);
            break;
        }
        default:
            TRACE("Invalid relay type: %u", relay_type);
            return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Successfully parsed pool relay");
    return PARSING_OK;
}

/// Helper to parse pool metadata (URL + hash or null)
static parser_status_e _parse_pool_metadata(buffer_t *buf, pool_metadata_t *metadata, bool *isNull) {
    TRACE("Parsing pool metadata");
    uint8_t metadata_present;
    if (!buffer_read_u8(buf, &metadata_present)) {
        TRACE("Failed to read metadata present flag");
        return CERTIFICATES_PARSING_ERROR;
    }

    if (metadata_present == 0) {
        *isNull = true;
        TRACE("Pool metadata is null");
        return PARSING_OK;
    }

    *isNull = false;
    // URL (length + data)
    uint8_t url_len;
    if (!buffer_read_u8(buf, &url_len)) {
        TRACE("Failed to read metadata URL length");
        return CERTIFICATES_PARSING_ERROR;
    }
    metadata->urlSize = url_len;

    if (url_len > MAX_ANCHOR_URL_LENGTH) {
        TRACE("Metadata URL length exceeds maximum: %u > %u", url_len, MAX_ANCHOR_URL_LENGTH);
        return CERTIFICATES_PARSING_ERROR;
    }

    if (!buffer_read_bytes_ptr(buf, &metadata->url, url_len)) {
        TRACE("Failed to read metadata URL");
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(metadata->url != NULL);
    TRACE("Metadata URL length: %u", url_len);
    if (!str_isPrintableAsciiWithoutSpaces(metadata->url, metadata->urlSize)) {
        TRACE("Metadata URL contains non-printable ASCII or spaces");
        return CERTIFICATES_PARSING_ERROR;
    }

    // Hash (32 bytes for blake2b-256)
    if (!buffer_read_bytes_ptr(buf, &metadata->hash, ANCHOR_HASH_LENGTH)) {
        TRACE("Failed to read metadata hash");
        return CERTIFICATES_PARSING_ERROR;
    }
    ASSERT(metadata->hash != NULL);
    TRACE("Successfully parsed pool metadata");
    return PARSING_OK;
}

/// Parse CERTIFICATE_STAKE_POOL_REGISTRATION
parser_status_e parse_certificate_stake_pool_registration(buffer_t *buf,
                                                         certificate_data_t *cert_data) {
    TRACE("Parsing STAKE_POOL_REGISTRATION certificate");
    cert_data->type = CERTIFICATE_STAKE_POOL_REGISTRATION;

    // Parse pool ID (operator key - hash or path)
    parser_status_e status = _parse_pool_id(buf, &cert_data->poolId);
    if (status != PARSING_OK) {
        TRACE("Failed to parse pool ID");
        return status;
    }

    // Parse VRF key hash (32 bytes)
    if (!buffer_read_bytes_ptr(buf, &cert_data->vrfKeyHash, VRF_KEY_HASH_LENGTH)) {
        TRACE("Failed to read VRF key hash");
        return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Successfully parsed VRF key hash");

    // Parse financials
    if (!buffer_read_u64(buf, &cert_data->poolRegistration.pledge, BE)) {
        TRACE("Failed to read pledge");
        return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Pledge: ");
    TRACE_UINT64(cert_data->poolRegistration.pledge);

    if (!buffer_read_u64(buf, &cert_data->poolRegistration.cost, BE)) {
        TRACE("Failed to read cost");
        return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Cost: ");
    TRACE_UINT64(cert_data->poolRegistration.cost);

    // Parse margin (unit_interval: numerator + denominator)
    if (!buffer_read_u64(buf, &cert_data->poolRegistration.marginNumerator, BE)) {
        TRACE("Failed to read margin numerator");
        return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Margin numerator: ");
    TRACE_UINT64(cert_data->poolRegistration.marginNumerator);

    if (!buffer_read_u64(buf, &cert_data->poolRegistration.marginDenominator, BE)) {
        TRACE("Failed to read margin denominator");
        return CERTIFICATES_PARSING_ERROR;
    }
    if (cert_data->poolRegistration.marginDenominator == 0) {
        TRACE("Invalid margin denominator: cannot be zero");
        return CERTIFICATES_PARSING_ERROR;
    }
    if (cert_data->poolRegistration.marginNumerator > cert_data->poolRegistration.marginDenominator) {
        TRACE("Invalid margin: numerator > denominator");
        TRACE_UINT64(cert_data->poolRegistration.marginNumerator);
        TRACE_UINT64(cert_data->poolRegistration.marginDenominator);
        return CERTIFICATES_PARSING_ERROR;
    }
    TRACE("Margin denominator: ");
    TRACE_UINT64(cert_data->poolRegistration.marginDenominator);

    // Parse reward account (hash or path)
    uint8_t reward_account_type;
    if (!buffer_read_u8(buf, &reward_account_type)) {
        TRACE("Failed to read reward account type");
        return CERTIFICATES_PARSING_ERROR;
    }

    TRACE("Reward account type wire: 0x%02x", reward_account_type);
    switch (reward_account_type) {
        case 0x00:  // KEY_HASH
            cert_data->poolRegistration.rewardAccount.keyReferenceType = KEY_REFERENCE_HASH;
            if (!buffer_read_bytes_ptr(buf, &cert_data->poolRegistration.rewardAccount.hashBuffer,
                                       REWARD_ACCOUNT_LENGTH)) {
                TRACE("Failed to read reward account hash");
                return CERTIFICATES_PARSING_ERROR;
            }
            ASSERT(cert_data->poolRegistration.rewardAccount.hashBuffer != NULL);
            TRACE("Successfully parsed reward account as KEY_HASH");
            break;
        case 0x02:  // KEY_PATH
            cert_data->poolRegistration.rewardAccount.keyReferenceType = KEY_REFERENCE_PATH;
            if (!buffer_read_bip44_path(buf, &cert_data->poolRegistration.rewardAccount.path)) {
                TRACE("Failed to read reward account path");
                return CERTIFICATES_PARSING_ERROR;
            }
            TRACE("Successfully parsed reward account as KEY_PATH");
            break;
        default:
            TRACE("Invalid reward account type: 0x%02x", reward_account_type);
            return CERTIFICATES_PARSING_ERROR;
    }

    // Parse pool owners array
    uint8_t num_owners;
    if (!buffer_read_u8(buf, &num_owners)) {
        TRACE("Failed to read number of pool owners");
        return CERTIFICATES_PARSING_ERROR;
    }
    cert_data->poolRegistration.numPoolOwners = num_owners;
    TRACE("Number of pool owners: %u", num_owners);

    // Parse each pool owner
    cert_data->poolRegistration.poolOwners = NULL;
    for (uint16_t i = 0; i < num_owners; i++) {
        TRACE("Parsing pool owner %u", i);
        tx_certificate_node_t *owner_item = (tx_certificate_node_t *) app_mem_alloc(sizeof(tx_certificate_node_t));
        if (owner_item == NULL) {
            TRACE("Failed to allocate memory for pool owner");
            return CERTIFICATES_PARSING_ERROR;
        }

        status = parse_stake_credential(buf, &owner_item->certificate.stakeCredential);
        if (status != PARSING_OK) {
            TRACE("Failed to parse pool owner credential");
            app_mem_free(owner_item);
            return status;
        }

        // Add to linked list
        owner_item->flist_node.next = NULL;
        flist_push_back(&cert_data->poolRegistration.poolOwners, &owner_item->flist_node);
    }

    // Parse relays array
    uint8_t num_relays;
    if (!buffer_read_u8(buf, &num_relays)) {
        TRACE("Failed to read number of relays");
        return CERTIFICATES_PARSING_ERROR;
    }
    cert_data->poolRegistration.numRelays = num_relays;
    TRACE("Number of relays: %u", num_relays);

    // Parse each relay
    cert_data->poolRegistration.relays = NULL;
    for (uint16_t i = 0; i < num_relays; i++) {
        TRACE("Parsing relay %u", i);
        tx_certificate_node_t *relay_item = (tx_certificate_node_t *) app_mem_alloc(sizeof(tx_certificate_node_t));
        if (relay_item == NULL) {
            TRACE("Failed to allocate memory for relay");
            return CERTIFICATES_PARSING_ERROR;
        }

        // We need to get the certificate_data properly typed for relay
        // The certificate_data union contains pool_relay_t space
        pool_relay_t *relay = (pool_relay_t *) &relay_item->certificate;

        status = _parse_pool_relay(buf, relay);
        if (status != PARSING_OK) {
            TRACE("Failed to parse relay");
            app_mem_free(relay_item);
            return status;
        }

        // Add to linked list
        relay_item->flist_node.next = NULL;
        flist_push_back(&cert_data->poolRegistration.relays, &relay_item->flist_node);
    }

    // Parse pool metadata
    status = _parse_pool_metadata(buf, &cert_data->poolRegistration.poolMetadata,
                                 &cert_data->poolRegistration.poolMetadataIsNull);
    if (status != PARSING_OK) {
        TRACE("Failed to parse pool metadata");
        return status;
    }

    TRACE("Successfully parsed STAKE_POOL_REGISTRATION");
    return PARSING_OK;
}
