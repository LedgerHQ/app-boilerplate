#include "utils/utils.h"
#include "buffer.h"
#include "derive_address.h"
#include "deriveAddress/deriveAddress_types.h"
#include "globals.h"
#include "addressUtils/addressUtilsShelley.h"
#include "securityPolicy.h"
#include "utils/assert.h"
#include "nbgl_use_case.h"

#include "io.h"

#include "ux.h"
#include "utils.h"
#include "os_io_seproxyhal.h"
#include "display.h"

static void prepareResponse() {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    ctx->address.size =
        deriveAddress(&ctx->addressParams, ctx->address.buffer, SIZEOF(ctx->address.buffer));
    ctx->responseReadyMagic = RESPONSE_READY_MAGIC;
}

int handler_derive_address(buffer_t *cdata, uint8_t display_type) {

    explicit_bzero(&G_context, sizeof(G_context));
    G_context.req_type = REQUEST_EXPORT_PUBKEY;


    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    ctx->responseReadyMagic = 0;
    bool is_parsed = buffer_parseAddressParams(cdata, &ctx->addressParams);
    if (!is_parsed) {
        return RETURN_BAD_PARSE;
    }

    switch (display_type) {
        case P1_RETURN: {
            security_policy_t policy = policyForReturnDeriveAddress(&ctx->addressParams);
            TRACE("RETURN");
            TRACE("Policy: %d", (int) policy);
            if (policy == POLICY_DENY) return RETURN_POLICY_DENY;
            prepareResponse();
            return ui_deriveAddress_handleReturn(policy);
        }
        case P1_DISPLAY: {
            security_policy_t policy = policyForShowDeriveAddress(&ctx->addressParams);
            TRACE("DISPLAY");
            TRACE("Policy: %d", (int) policy);
            if (policy == POLICY_DENY) return RETURN_POLICY_DENY;
            prepareResponse();
            return ui_deriveAddress_handleDisplay(policy);
        }    
        default:
            return RETURN_BAD_PARSE;
    }
}