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

#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "os.h"
#include "glyphs.h"
#include "nbgl_use_case.h"
#include "io.h"
#include "addressUtils/bip44.h"
#include "format.h"

#include "ui/ui_icons.h"
#include "cardano_constants.h"
#include "globals.h"
#include "utils/utils.h"
#include "app_context.h"
#include "cardano_swo.h"
#include "opcert_types.h"
#include "menu.h"
#include "securityPolicy.h"
#include "derive_address.h"
#include "memory/mem_utils.h"
#include "ui_utils.h"
#include "handler/derive_address.h"
#include "ui_display_address_derivation.h"
#include "tx_ui_helpers.h"
/**
 * Cleanup dynamically allocated buffers
 */
static void derive_address_buffer_cleanup(void) {
    // Cleanup all tracked allocations (all string buffers and warning structure)
    ui_cleanup_tracked_allocations();
    // Cleanup the pairs array
    ui_pairs_cleanup();
}

static void derive_address_review_choice(bool confirm) {
    // TODO: choose only one cleanup
    derive_address_buffer_cleanup();
    TRACE("derive_address_buffer_cleanup\n");
    if (confirm) {
        nbgl_useCaseStatus("Confirm\n address export", true, ui_menu_main);
        io_send_response_pointer(NULL, 0, SWO_SUCCESS);
        TRACE("User confirmed - showing signed status");
    } else {
        TRACE("User rejected - showing rejected status");
        nbgl_useCaseStatus("Address\nrejected", true, ui_menu_main);
    }
    derive_address_buffer_cleanup();
}

static void respond_with_address_success(ins_derive_address_ctx_t *ctx) {
    ctx->responseReadyMagic = 0;
    ASSERT(ctx->address.size <= sizeof(ctx->address.buffer));
    io_send_response_pointer(ctx->address.buffer, ctx->address.size, SWO_SUCCESS);
}

static void derive_address_return_review_choice(bool confirm) {
    if (confirm) {
        TRACE("User confirmed - showing signed status");
        ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
        nbgl_useCaseStatus("Confirm\n address export", true, ui_menu_main);
        respond_with_address_success(ctx);
    } else {
        TRACE("User rejected - showing rejected status");
        nbgl_useCaseStatus("Address\nrejected", true, ui_menu_main);
    }
    derive_address_buffer_cleanup();
}

// TODO: verify that warning_bits_t requires one extra UI pair
static bool prepare_address_ui_pairs(
    const addressParams_t *params,
    warning_bits_t warnings
) {
    const bool hasWarning = (warnings != 0);

#define PAYMENT_INFO_SIZE MAX(MAX_BECH32_STRING_LENGTH, MAX_BIP44_PATH_STRING_LENGTH)

    ui_reset_error_status();

    const bool isRewardAddress =
        (params->type == REWARD_KEY || params->type == REWARD_SCRIPT);

    const bool isEnterpriseAddress =
        (params->type == ENTERPRISE_KEY || params->type == ENTERPRISE_SCRIPT);

    // Calculate number of UI pairs
    int pairCount = isRewardAddress ? 1 : 2;
    if (hasWarning) {
        pairCount += 1;
    }

    if (!ui_pairs_init(pairCount)) {
        return false;
    }

    // Add warning banner first, if needed
    if (hasWarning) {
        TRACE("Adding warning banner");
        UI_ADD_STATIC(
            UI_STATIC_LABEL("Warning:"),
            UI_STATIC_LABEL("Unusual request\nProceed with care")
        );
    }

    // Add address-specific UI pairs
    if (isRewardAddress) {
        addStakingInfoUIPairs(params);
    } else if (isEnterpriseAddress) {
        addPaymentInfoUIPairs(params);
        UI_ADD_STATIC(
            UI_STATIC_LABEL("Warning:"),
            UI_STATIC_LABEL("No staking rewards")
        );
    } else {
        TRACE("Adding both payment and staking info");
        addPaymentInfoUIPairs(params);
        addStakingInfoUIPairs(params);
    }

    return true;
}


static void ui_displayExportAddress(warning_bits_t warnings) {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    prepare_address_ui_pairs(&ctx->addressParams, warnings);

    static char humanAddress[MAX_HUMAN_ADDRESS_SIZE] = {0};
    format_address_human_readable(ctx->address.buffer,
                                  ctx->address.size,
                                  humanAddress,
                                  SIZEOF(humanAddress));
    // TODO: mismatch with old app
    //- no warning banner for byron addresses
    nbgl_useCaseAddressReview(humanAddress,
                              g_pairsList,
                              &ICON_APP_CARDANO,
                              "Verify Cardano address",
                              NULL,
                              derive_address_review_choice);
    return;
}

static void ui_returnExportAddress(warning_bits_t warnings) {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    prepare_address_ui_pairs(&ctx->addressParams, warnings);

    // TODO: mismatch with old app
    //- no warning banner for byron addresses
    nbgl_useCaseReviewLight(TYPE_OPERATION,
                            g_pairsList,
                            &ICON_APP_CARDANO,
                            "Export address",
                            NULL,
                            "Confirm\n address export",
                            derive_address_return_review_choice);
    return;
}

void deriveAddress_return_ui_runStep(warning_bits_t warnings) {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;

    TRACE("step %d", ctx->ui_step);
    ASSERT(ctx->responseReadyMagic == RESPONSE_READY_MAGIC);

    switch (ctx->ui_step) {
        case RETURN_UI_STEP_BEGIN:
            ctx->ui_step = RETURN_UI_STEP_RESPOND;
            ui_returnExportAddress(warnings);
            break;

        case RETURN_UI_STEP_RESPOND:
            respond_with_address_success(ctx);
            break;

        default:
            // TODO: check if status is appropiate
            send_swo_and_reset(SWO_BAD_STATE);
            break;
    }

    return;
}

void deriveAddress_display_ui_runStep(warning_bits_t warnings) {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;

    ASSERT(ctx->responseReadyMagic == RESPONSE_READY_MAGIC);

    switch (ctx->ui_step) {
        case DISPLAY_UI_STEP_BEGIN:
            ctx->ui_step = DISPLAY_UI_STEP_RESPOND;
            ui_displayExportAddress(warnings);
            break;

        case DISPLAY_UI_STEP_RESPOND:
            io_send_response_pointer(NULL, 0, SWO_SUCCESS);
            break;

        default:
            // TODO: check if status is appropiate
            send_swo_and_reset(SWO_BAD_STATE);
            break;
    }

    return;
}

void ui_deriveAddress_handleReturn(security_policy_t policy, warning_bits_t warnings) {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    switch (policy) {
        case POLICY_SHOW:
            ctx->ui_step = RETURN_UI_STEP_BEGIN;
            break;
        case POLICY_HIDE:
            ctx->ui_step = RETURN_UI_STEP_RESPOND;
            break;
        default:
            break;
    }
    deriveAddress_return_ui_runStep(warnings);
    return;
}

void ui_deriveAddress_handleDisplay(security_policy_t policy, warning_bits_t warnings) {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    switch (policy) {
        case POLICY_SHOW:
            ctx->ui_step = DISPLAY_UI_STEP_BEGIN;
            break;
        default:
            break;
    }
    deriveAddress_display_ui_runStep(warnings);
    return;
}