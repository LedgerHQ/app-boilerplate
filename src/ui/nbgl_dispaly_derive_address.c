#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "os.h"
#include "glyphs.h"
#include "nbgl_use_case.h"
#include "io.h"
#include "addressUtils/bip44.h"
#include "format.h"

#include "display.h"
#include "cardano_constants.h"
#include "globals.h"
#include "utils/utils.h"
#include "utils/cardano_os_utils.h"
#include "cardano_swo.h"
#include "opcert_types.h"
#include "menu.h"
#include "securityPolicy.h"
#include "nbgl_screens.h"
#include "derive_address.h"
#include "memory/mem_utils.h"
#include "ui_utils.h"
#include "handler/derive_address.h"

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
    TRACE("respond_with_address_success");

    ctx->responseReadyMagic = 0;

    // If buffer is used as a C-string elsewhere, consider '<' instead of '<='
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

typedef void ui_callback_fn_t();

/* ========================== DISPLAY ADDRESS ========================== */
// TODO: uncomment
void respond_with_user_reject() {
    // io_send_buf(ERR_REJECTED_BY_USER, NULL, 0);
    //  Change to user reject
    io_send_response_pointer(NULL, 0, SWO_UNKNOWN);
    // ui_idle();
}

/* ========================== RETURN ADDRESS ========================== */

static int prepare_address_info_pairs(const ins_derive_address_ctx_t *ctx) {
#define PAYMENT_INFO_SIZE MAX(BECH32_STRING_SIZE_MAX, BIP44_PATH_STRING_SIZE_MAX)

    static char line1[30] = {0};
    static char paymentInfo[PAYMENT_INFO_SIZE] = {0};
    static char line2[30] = {0};
    static char stakingInfo[120] = {0};

    TRACE("type: %d\n", ctx->addressParams.type);

    if (ctx->addressParams.type != REWARD_KEY && ctx->addressParams.type != REWARD_SCRIPT) {
        ui_getPaymentInfoScreen(line1,
                                SIZEOF(line1),
                                paymentInfo,
                                SIZEOF(paymentInfo),
                                &ctx->addressParams);
    }

    ui_getStakingInfoScreen(line2,
                            SIZEOF(line2),
                            stakingInfo,
                            SIZEOF(stakingInfo),
                            &ctx->addressParams);

    if (ctx->addressParams.type == REWARD_KEY || ctx->addressParams.type == REWARD_SCRIPT) {
        if (!ui_pairs_init(1)) {
            return -1;
        }

        g_pairs[0].item = line2;
        g_pairs[0].value = stakingInfo;
    } else {
        if (!ui_pairs_init(2)) {
            return -1;
        }

        g_pairs[0].item = line1;
        g_pairs[0].value = paymentInfo;
        g_pairs[1].item = line2;
        g_pairs[1].value = stakingInfo;
    }

    return 0;
}

static int ui_displayExportAddress() {
#define PAYMENT_INFO_SIZE MAX(BECH32_STRING_SIZE_MAX, BIP44_PATH_STRING_SIZE_MAX)

    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    static char humanAddress[MAX_HUMAN_ADDRESS_SIZE] = {0};
    ui_getAddressScreen(humanAddress, SIZEOF(humanAddress), ctx->address.buffer, ctx->address.size);

    if (prepare_address_info_pairs(ctx) != 0) {
        TRACE("Failed to initialize pairs");
        derive_address_buffer_cleanup();
        send_error_and_reset(SWO_DISPLAY_AMOUNT_FAIL);
        return -1;
    }
    /*nbgl_useCaseReviewLight(TYPE_OPERATION,
                            g_pairsList,
                            &ICON_APP_CARDANO,
                            "Derive address",
                            NULL,
                            "Address",
                            derive_address_review_choice);*/
    nbgl_useCaseAddressReview(humanAddress,
                              g_pairsList,
                              &ICON_APP_CARDANO,
                              "Confirm address",
                              NULL,
                              derive_address_review_choice);
    return 0;
}

static int ui_returnExportAddress() {
#define PAYMENT_INFO_SIZE MAX(BECH32_STRING_SIZE_MAX, BIP44_PATH_STRING_SIZE_MAX)

    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    static char humanAddress[MAX_HUMAN_ADDRESS_SIZE] = {0};
    ui_getAddressScreen(humanAddress, SIZEOF(humanAddress), ctx->address.buffer, ctx->address.size);

    if (prepare_address_info_pairs(ctx) != 0) {
        TRACE("Failed to initialize pairs");
        derive_address_buffer_cleanup();
        send_error_and_reset(SWO_DISPLAY_AMOUNT_FAIL);
        return -1;
    }

    nbgl_useCaseAddressReview(humanAddress,
                              g_pairsList,
                              &ICON_APP_CARDANO,
                              "Confirm\n address export",
                              NULL,
                              derive_address_return_review_choice);
    return 0;
}

int deriveAddress_return_ui_runStep(void) {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;

    TRACE("step %d", ctx->ui_step);
    ASSERT(ctx->responseReadyMagic == RESPONSE_READY_MAGIC);

    switch (ctx->ui_step) {
        /*case RETURN_UI_STEP_WARNING:
            ctx->ui_step = RETURN_UI_STEP_BEGIN;
            ui_displayUnusualWarning(returnCallback);
            // ui_displayUnusualWarning(returnCallback);
            break;
        */

        case RETURN_UI_STEP_BEGIN:
            ctx->ui_step = RETURN_UI_STEP_RESPOND;
            return ui_returnExportAddress();
            break;

        case RETURN_UI_STEP_RESPOND:
            TRACE("DIRECT RESPOND");
            respond_with_address_success(ctx);
            break;

        default:
            // TODO: check
            // ASSERT(false);
            send_error_and_reset(SWO_DISPLAY_ADDRESS_FAIL);
            return -1;
            break;
    }

    return 0;
}

int deriveAddress_display_ui_runStep(void) {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;

    ASSERT(ctx->responseReadyMagic == RESPONSE_READY_MAGIC);

    switch (ctx->ui_step) {
        /*case DISPLAY_UI_STEP_WARNING:
            ctx->ui_step = DISPLAY_UI_STEP_PAYMENT_INFO;
            ui_displayUnusualWarning(displayCallback);
            break;*/

        case DISPLAY_UI_STEP_BEGIN:
            ctx->ui_step = DISPLAY_UI_STEP_RESPOND;
            return ui_displayExportAddress();
            break;

        case DISPLAY_UI_STEP_RESPOND:
            io_send_response_pointer(NULL, 0, SWO_SUCCESS);
            break;

        default:
            // TODO: check
            // ASSERT(false);
            send_error_and_reset(SWO_DISPLAY_ADDRESS_FAIL);
            return -1;
            break;
    }

    return 0;
}

int ui_deriveAddress_handleReturn(security_policy_t policy) {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    switch (policy) {
        case POLICY_SHOW:
            ctx->ui_step = RETURN_UI_STEP_BEGIN;
            break;

        case POLICY_HIDE:
            ctx->ui_step = RETURN_UI_STEP_RESPOND;
            break;

        default:
            return -1;
    }
    return deriveAddress_return_ui_runStep();
}

int ui_deriveAddress_handleDisplay(security_policy_t policy) {
    ins_derive_address_ctx_t *ctx = &G_context.derive_address_info;
    switch (policy) {
        case POLICY_SHOW:
            ctx->ui_step = DISPLAY_UI_STEP_BEGIN;
            break;

        default:
            return -1;
    }
    return deriveAddress_display_ui_runStep();
}