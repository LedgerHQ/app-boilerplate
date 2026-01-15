#include "utils/utils.h"
#include "buffer.h"
#include "derive_address.h"
#include "deriveAddress/deriveAddress_types.h"
#include "cardano_swo.h"
#include "globals.h"
#include "addressUtils/addressUtilsShelley.h"
#include "securityPolicy.h"
#include "utils/assert.h"
#include "nbgl_use_case.h"
#include "app_context.h"

#include "io.h"

#include "ux.h"
#include "utils.h"
#include "os_io_seproxyhal.h"
#include "ui_display_address_derivation.h"

static void prepareResponse() {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    ctx->address.size =
        deriveAddress(&ctx->addressParams, ctx->address.buffer, SIZEOF(ctx->address.buffer));
    // TODO: modify usage of responseReadyMagic ?
    ctx->responseReadyMagic = RESPONSE_READY_MAGIC;
}

void handler_derive_address(buffer_t *cdata, uint8_t display_type) {

    explicit_bzero(&G_context, sizeof(G_context));

    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    ctx->responseReadyMagic = 0;
    bool is_parsed = buffer_parseAddressParams(cdata, &ctx->addressParams);
    if (!is_parsed) {
        send_swo_and_reset(SWO_BAD_STATE);
        return;
    }

    switch (display_type) {
        case P1_RETURN: {
            security_policy_t policy = policyForReturnDeriveAddress(&ctx->addressParams);
            TRACE("RETURN");
            TRACE("Policy: %d", (int) policy);
            if (policy == POLICY_DENY){
                LEDGER_ASSERT(false, "POLICY_DENY");
                return;
            }
            prepareResponse();
            ui_deriveAddress_handleReturn(policy);
            return;
        }
        case P1_DISPLAY: {
            security_policy_t policy = policyForShowDeriveAddress(&ctx->addressParams);
            TRACE("DISPLAY");
            TRACE("Policy: %d", (int) policy);
            if (policy == POLICY_DENY){
                LEDGER_ASSERT(false, "POLICY_DENY");
                return;
            }
            prepareResponse();
            ui_deriveAddress_handleDisplay(policy);
            return;
        }    
        default:
            LEDGER_ASSERT(false, "display type should be handled before");
            return;
    }
}