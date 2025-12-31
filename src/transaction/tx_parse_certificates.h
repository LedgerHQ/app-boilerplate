#pragma once

#include "buffer.h"
#include "transaction/tx.h"
#include "tx_parse.h"

/**
 * Parse a complete stake credential (type + data)
 *
 * Wire format for credential type (as sent by Python client):
 * - 0x00: KEY_HASH (28-byte public key hash)
 * - 0x01: SCRIPT_HASH (28-byte script hash)
 * - 0x02: KEY_PATH (device derivation path; converted to KEY_HASH before hashing)
 *
 * CBOR credential types (tx hash builder, per CDDL):
 * - 0x00: KEY_HASH
 * - 0x01: SCRIPT_HASH
 *
 * @param[in]  buf        Buffer with serialized credential
 * @param[out] credential Parsed credential structure
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_stake_credential(buffer_t *buf, ext_credential_t *credential);

/**
 * Parse CERTIFICATE_STAKE_REGISTRATION or CERTIFICATE_STAKE_DEREGISTRATION (Shelley era)
 *
 * Format:
 * - certificate_type (1 byte): 0 or 1
 * - stake_credential (variable): type + data
 *
 * @param[in]  buf      Buffer with serialized certificate
 * @param[in]  cert_type Either CERTIFICATE_STAKE_REGISTRATION or CERTIFICATE_STAKE_DEREGISTRATION
 * @param[out] cert_data Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_stake_registration_deregistration(
    buffer_t *buf,
    certificate_type_t cert_type,
    certificate_data_t *cert_data);

/**
 * Parse CERTIFICATE_STAKE_DELEGATION
 *
 * Format:
 * - certificate_type (1 byte): 2
 * - stake_credential (variable): type + data
 * - pool_key_hash (28 bytes)
 *
 * @param[in]  buf      Buffer with serialized certificate
 * @param[out] cert_data Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_stake_delegation(buffer_t *buf,
                                                  certificate_data_t *cert_data);

/**
 * Parse CERTIFICATE_STAKE_REGISTRATION_CONWAY or CERTIFICATE_STAKE_DEREGISTRATION_CONWAY
 *
 * Format:
 * - certificate_type (1 byte): 7 or 8
 * - stake_credential (variable): type + data
 * - deposit (8 bytes): deposit amount in lovelace
 *
 * @param[in]  buf       Buffer with serialized certificate
 * @param[in]  cert_type Either CERTIFICATE_STAKE_REGISTRATION_CONWAY or CERTIFICATE_STAKE_DEREGISTRATION_CONWAY
 * @param[out] cert_data  Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_stake_registration_deregistration_conway(
    buffer_t *buf,
    certificate_type_t cert_type,
    certificate_data_t *cert_data);

/**
 * Parse CERTIFICATE_STAKE_POOL_RETIREMENT
 *
 * Format:
 * - certificate_type (1 byte): 4
 * - pool_key_path (variable): BIP44 derivation path
 * - retirement_epoch (8 bytes): epoch number
 *
 * @param[in]  buf      Buffer with serialized certificate
 * @param[out] cert_data Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_stake_pool_retirement(buffer_t *buf,
                                                       certificate_data_t *cert_data);

/**
 * Parse CERTIFICATE_VOTE_DELEGATION
 *
 * Format:
 * - certificate_type (1 byte): 9
 * - stake_credential (variable): type + data
 * - drep (variable): DRep specification (key hash, key path, script hash, abstain, or no confidence)
 *
 * @param[in]  buf      Buffer with serialized certificate
 * @param[out] cert_data Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_vote_delegation(buffer_t *buf,
                                                 certificate_data_t *cert_data);

/**
 * Parse CERTIFICATE_AUTHORIZE_COMMITTEE_HOT
 *
 * Format:
 * - certificate_type (1 byte): 14
 * - cold_credential (variable): committee cold key credential
 * - hot_credential (variable): committee hot key credential
 *
 * @param[in]  buf      Buffer with serialized certificate
 * @param[out] cert_data Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_authorize_committee_hot(buffer_t *buf,
                                                         certificate_data_t *cert_data);

/**
 * Parse CERTIFICATE_RESIGN_COMMITTEE_COLD
 *
 * Format:
 * - certificate_type (1 byte): 15
 * - cold_credential (variable): committee cold key credential
 * - anchor (variable): optional anchor (URL + hash)
 *
 * @param[in]  buf      Buffer with serialized certificate
 * @param[out] cert_data Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_resign_committee_cold(buffer_t *buf,
                                                       certificate_data_t *cert_data);

/**
 * Parse CERTIFICATE_DREP_REGISTRATION
 *
 * Format:
 * - certificate_type (1 byte): 16
 * - drep_credential (variable): DRep key credential
 * - deposit (8 bytes): deposit amount
 * - anchor (variable): anchor (URL + hash)
 *
 * @param[in]  buf      Buffer with serialized certificate
 * @param[out] cert_data Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_drep_registration(buffer_t *buf,
                                                   certificate_data_t *cert_data);

/**
 * Parse CERTIFICATE_DREP_DEREGISTRATION
 *
 * Format:
 * - certificate_type (1 byte): 17
 * - drep_credential (variable): DRep key credential
 * - deposit (8 bytes): deposit amount
 *
 * @param[in]  buf      Buffer with serialized certificate
 * @param[out] cert_data Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_drep_deregistration(buffer_t *buf,
                                                     certificate_data_t *cert_data);

/**
 * Parse CERTIFICATE_DREP_UPDATE
 *
 * Format:
 * - certificate_type (1 byte): 18
 * - drep_credential (variable): DRep key credential
 * - anchor (variable): anchor (URL + hash)
 *
 * @param[in]  buf      Buffer with serialized certificate
 * @param[out] cert_data Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_drep_update(buffer_t *buf,
                                             certificate_data_t *cert_data);

/**
 * Parse CERTIFICATE_STAKE_POOL_REGISTRATION
 *
 * Format:
 * - certificate_type (1 byte): 3
 * - pool_id (variable): operator key hash or path
 * - vrf_keyhash (32 bytes): VRF key hash
 * - pledge (8 bytes): pledge amount in lovelace
 * - cost (8 bytes): pool cost in lovelace
 * - margin (variable): unit interval (numerator + denominator)
 * - reward_account (29 bytes): reward account address
 * - pool_owners (variable): array of owner credentials
 * - relays (variable): array of relay specifications
 * - pool_metadata (variable): metadata URL and hash or null
 *
 * @param[in]  buf      Buffer with serialized certificate
 * @param[out] cert_data Parsed certificate data
 *
 * @return PARSING_OK on success, CERTIFICATES_PARSING_ERROR on failure
 */
parser_status_e parse_certificate_stake_pool_registration(buffer_t *buf,
                                                         certificate_data_t *cert_data);
