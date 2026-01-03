#include <stdbool.h>  // bool

#include "os.h"
#include "glyphs.h"
#include "nbgl_use_case.h"
#include "io.h"
#include "utils/utils.h"

#include "display.h"
#include "globals.h"
#include "cardano_swo.h"
#include "menu.h"
#include "transaction/tx_parse.h"
#include "ui_utils.h"
#include "utils/cardano_os_utils.h"

void tx_review_cleanup(void) {
    ui_cleanup_tracked_allocations();
    ui_pairs_cleanup();
    ui_clear_prepared_warning();
}

static void tx_review_choice(bool confirm) {
    if (confirm) {
        G_context.state.tx_state = TX_STATE_APPROVED;
        G_context.tx_info.current_witness = 0;
        io_send_response_pointer(G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash), SWO_SUCCESS);

        if (G_context.tx_info.num_witnesses > 0) {
            tx_review_cleanup();
            TRACE("Calling nbgl_useCaseSpinner(\"Processing\")");
            nbgl_useCaseSpinner("Processing");
        } else {
            tx_review_cleanup();
            tx_context_cleanup();
            G_context.state.tx_state = TX_STATE_NONE;
            G_context.req_type = REQUEST_NONE;
            TRACE("Calling nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main)");
            nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
        }
    } else {
        tx_review_cleanup();
        tx_context_cleanup();
        G_context.state.tx_state = TX_STATE_NONE;
        G_context.req_type = REQUEST_NONE;
        io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
        TRACE("Calling nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_menu_main)");
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_menu_main);
    }
}

int ui_display_transaction(void) {
    if (G_context.req_type != REQUEST_SIGN_TRANSACTION || G_context.state.tx_state != TX_STATE_UI_PREPARED) {
        G_context.state.tx_state = TX_STATE_NONE;
        return send_error_and_reset(SWO_BAD_STATE);
    }

    const char *review_subtitle = NULL;
    if (G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_PLUTUS_TX) {
        review_subtitle = "Plutus execution";
    }

    const nbgl_warning_t *warningPtr = ui_get_prepared_warning();
    if (warningPtr != NULL) {
        TRACE("Calling nbgl_useCaseAdvancedReview(TYPE_TRANSACTION)");
        nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                                   g_pairsList,
                                   &ICON_APP_CARDANO,
                                   "Review transaction",
                                   review_subtitle,
                                   "Sign transaction",
                                   NULL,
                                   warningPtr,
                                   tx_review_choice);
    } else {
        TRACE("Calling nbgl_useCaseReview(TYPE_TRANSACTION)");
        nbgl_useCaseReview(TYPE_TRANSACTION,
                           g_pairsList,
                           &ICON_APP_CARDANO,
                           "Review transaction",
                           review_subtitle,
#ifdef SCREEN_SIZE_WALLET
                           "Sign transaction",
#else
                           NULL,
#endif
                           tx_review_choice);
    }

    return 0;
}
