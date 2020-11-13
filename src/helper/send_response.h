#pragma once

#include "os.h"

#include "../context.h"

#define MEMBER_SIZE(type, member) (sizeof(((type *) 0)->member))
#define PUBKEY_LEN                (MEMBER_SIZE(cx_ecfp_public_key_t, W))
#define CHAINCODE_LEN             (MEMBER_SIZE(pubkey_ctx_t, chain_code))

int helper_send_response_pubkey(pubkey_ctx_t *pk_ctx);
