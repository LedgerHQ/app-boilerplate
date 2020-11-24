#pragma once

#include "os.h"

#include "../common/macros.h"

#define PUBKEY_LEN    (MEMBER_SIZE(pubkey_ctx_t, raw_public_key))
#define CHAINCODE_LEN (MEMBER_SIZE(pubkey_ctx_t, chain_code))

int helper_send_response_pubkey(void);

int helper_send_response_sig(void);
