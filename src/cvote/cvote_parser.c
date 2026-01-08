#include "cvote/cvote_parser.h"

#include <stdint.h>
#include <string.h>

#include "addressUtils/addressUtilsShelley.h"
#include "memory/mem.h"
#include "utils/buffer_utils.h"
#include "utils/utils.h"

#define TX_OUTPUT_DESTINATION_THIRD_PARTY 0x01
#define TX_OUTPUT_DESTINATION_DEVICE_OWNED 0x02

static void trace_credential_path(const char *label, const bip44_path_t *path) {
    ASSERT(label != NULL);
    ASSERT(path != NULL);

    char path_str[MAX_BIP44_PATH_STRING_LENGTH + 2] = {0};
    bool formatted = format_bip44_path(path, path_str, sizeof(path_str));
    TRACE("%s path %s", label, formatted ? path_str : "<path format failed>");
}

cvote_parser_status_t cvote_parse_credential(buffer_t *buf,
                                                    ext_credential_t *credential,
                                                    const char *label) {
    ASSERT(buf != NULL);
    ASSERT(credential != NULL);
    ASSERT(label != NULL);

    uint8_t type = 0;
    if (!buffer_read_u8(buf, &type)) {
        TRACE("%s credential type missing", label);
        return CVOTE_PARSER_INVALID_FORMAT;
    }

    switch (type) {
        case 0x00:  // KEY_HASH
            credential->type = EXT_CREDENTIAL_KEY_HASH;
            STATIC_ASSERT(SIZEOF(credential->publicKey) == PUBLIC_KEY_SIZE,
                          "credential public key size mismatch");
            if (!buffer_read_bytes(buf, credential->publicKey, PUBLIC_KEY_SIZE)) {
                TRACE("%s key hash data truncated", label);
                return CVOTE_PARSER_INVALID_FORMAT;
            }
            TRACE("%s credential: raw key", label);
            break;
        case 0x01:  // SCRIPT_HASH
            credential->type = EXT_CREDENTIAL_SCRIPT_HASH;
            STATIC_ASSERT(SIZEOF(credential->scriptHash) == SCRIPT_HASH_LENGTH,
                          "credential script hash size mismatch");
            if (!buffer_read_bytes(buf, credential->scriptHash, SCRIPT_HASH_LENGTH)) {
                TRACE("%s script hash data truncated", label);
                return CVOTE_PARSER_INVALID_FORMAT;
            }
            TRACE("%s credential: script hash", label);
            break;
        case 0x02:  // KEY_PATH
            credential->type = EXT_CREDENTIAL_KEY_PATH;
            if (!buffer_read_bip44_path(buf, &credential->keyPath)) {
                TRACE("%s path data truncated", label);
                return CVOTE_PARSER_INVALID_FORMAT;
            }
            trace_credential_path(label, &credential->keyPath);
            break;
        default:
            TRACE("%s invalid credential type 0x%02x", label, type);
            return CVOTE_PARSER_INVALID_FORMAT;
    }

    return CVOTE_PARSER_OK;
}

static cvote_parser_status_t parse_third_party_destination(buffer_t *buf, cvote_destination_t *destination) {
    ASSERT(buf != NULL);
    ASSERT(destination != NULL);

    destination->third_party.length = 0;
    destination->third_party.buffer = NULL;

    uint16_t address_len = 0;
    if (!buffer_read_u16(buf, &address_len, BE)) {
        TRACE("CVote third-party address length missing");
        return CVOTE_PARSER_INVALID_FORMAT;
    }
    TRACE("CVote third-party destination len %u", address_len);

    if (address_len == 0) {
        return CVOTE_PARSER_OK;
    }

    uint8_t *buffer = (uint8_t *) app_mem_alloc(address_len);
    if (buffer == NULL) {
        TRACE("CVote third-party destination allocate failed");
        return CVOTE_PARSER_OUT_OF_MEMORY;
    }

    if (!buffer_read_bytes(buf, buffer, address_len)) {
        TRACE("CVote third-party destination truncated");
        return CVOTE_PARSER_INVALID_FORMAT;
    }

    destination->third_party.buffer = buffer;
    destination->third_party.length = address_len;
    return CVOTE_PARSER_OK;
}

cvote_parser_status_t cvote_parse_destination(buffer_t *buf, cvote_destination_t *destination) {
    ASSERT(buf != NULL);
    ASSERT(destination != NULL);

    uint8_t destination_type = 0;
    if (!buffer_read_u8(buf, &destination_type)) {
        TRACE("CVote destination data missing");
        return CVOTE_PARSER_INVALID_FORMAT;
    }

    TRACE("CVote destination payload type 0x%x", destination_type);

    if (destination_type == TX_OUTPUT_DESTINATION_THIRD_PARTY) {
        destination->is_third_party = true;
        cvote_parser_status_t dest_status = parse_third_party_destination(buf, destination);
        if (dest_status != CVOTE_PARSER_OK) {
            return dest_status;
        }
        TRACE("CVote destination: third-party payload");
        return CVOTE_PARSER_OK;
    }

    if (destination_type != TX_OUTPUT_DESTINATION_DEVICE_OWNED) {
        TRACE("Unsupported CVote destination type 0x%x", destination_type);
        return CVOTE_PARSER_INVALID_FORMAT;
    }

    destination->is_third_party = false;
    explicit_bzero(&destination->params, sizeof(destination->params));
    if (!buffer_parseAddressParams(buf, &destination->params)) {
        TRACE("CVote destination parsing failed");
        return CVOTE_PARSER_INVALID_FORMAT;
    }

    TRACE("CVote destination type 0x%x, staking %d", destination->params.type, destination->params.stakingDataSource);
    return CVOTE_PARSER_OK;
}

static cvote_parser_status_t parse_vote_credential(buffer_t *buf, cvote_aux_data_t *data) {
    ASSERT(buf != NULL);
    ASSERT(data != NULL);

    cvote_parser_status_t status =
        cvote_parse_credential(buf, &data->vote_credential, "Vote credential");
    if (status != CVOTE_PARSER_OK) {
        return status;
    }
    data->has_vote_credential = true;
    return CVOTE_PARSER_OK;
}

cvote_parser_status_t cvote_parse_aux_data_init(buffer_t *buf, cvote_aux_data_t **out_data) {
    ASSERT(buf != NULL);
    ASSERT(out_data != NULL);

    *out_data = NULL;
    if (buf->size - buf->offset < 3) {
        return CVOTE_PARSER_INVALID_FORMAT;
    }

    cvote_aux_data_t *data = (cvote_aux_data_t *) app_mem_alloc(sizeof(*data));
    if (data == NULL) {
        return CVOTE_PARSER_OUT_OF_MEMORY;
    }
    explicit_bzero(data, sizeof(*data));

    uint8_t format = 0;
    if (!buffer_read_u8(buf, &format) ||
        !buffer_read_u16(buf, &data->delegation_count, BE)) {
        return CVOTE_PARSER_INVALID_FORMAT;
    }

    switch (format) {
        case CIP15:
        case CIP36:
            data->format = (cvote_registration_format_t) format;
            break;
        default:
            return CVOTE_PARSER_INVALID_FORMAT;
    }

    if (cvote_parse_credential(buf, &data->staking_credential, "Staking credential") !=
        CVOTE_PARSER_OK) {
        return CVOTE_PARSER_INVALID_FORMAT;
    }

    cvote_parser_status_t dest_status = cvote_parse_destination(buf, &data->destination);
    if (dest_status != CVOTE_PARSER_OK) {
        return dest_status;
    }

    if (!buffer_read_u64(buf, &data->nonce, BE)) {
        return CVOTE_PARSER_INVALID_FORMAT;
    }

    if (data->format == CIP36) {
        if (!buffer_read_u64(buf, &data->voting_purpose, BE)) {
            return CVOTE_PARSER_INVALID_FORMAT;
        }
        data->has_voting_purpose = true;
        if (data->delegation_count == 0) {
            cvote_parser_status_t vote_status = parse_vote_credential(buf, data);
            if (vote_status != CVOTE_PARSER_OK) {
                return vote_status;
            }
        }
    } else {
        cvote_parser_status_t vote_status = parse_vote_credential(buf, data);
        if (vote_status != CVOTE_PARSER_OK) {
            return vote_status;
        }
    }

    if (buf->offset != buf->size) {
        TRACE("CVote init payload not fully consumed");
        return CVOTE_PARSER_INVALID_FORMAT;
    }

    *out_data = data;
    return CVOTE_PARSER_OK;
}
