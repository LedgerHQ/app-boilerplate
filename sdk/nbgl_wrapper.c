
#include "nbgl_use_case.h"
#include "ledger_assert.h"
#include "os_pic.h"
#include "os.h"
#include "glyphs.h"

#ifndef TARGET_NANOS
static nbgl_pageInfoLongPress_t infoLongPress;

static const nbgl_layoutTagValueList_t *review_tagValueList;
static const nbgl_icon_details_t *review_icon;
static const char *review_finish_page_text;

static const char *review_address;

static nbgl_choiceCallback_t review_choice_callback;


static void tx_confirm_transaction_rejection(void) {
    review_choice_callback(false);
}

static void tx_ask_transaction_rejection_confirmation(void) {
    // display a choice to confirm/cancel rejection
    nbgl_useCaseConfirm("Reject transaction?",
                        NULL,
                        "Yes, Reject",
                        "Go back to transaction",
                        tx_confirm_transaction_rejection);
}

static void tx_review_choice(bool confirm) {
    if (confirm) {
        review_choice_callback(confirm);
    } else {
        tx_ask_transaction_rejection_confirmation();
    }
}

static void tx_review_continue(void) {
    // Info long press
    infoLongPress.icon = review_icon;
    infoLongPress.text = review_finish_page_text;
    infoLongPress.longPressText = "Hold to sign";

    nbgl_useCaseStaticReview(review_tagValueList, &infoLongPress, "Reject transaction", tx_review_choice);
}
#endif

void nbgl_useCaseTransactionReview(
    const nbgl_layoutTagValueList_t *tagValueList,
    const nbgl_icon_details_t *icon,
    const char *reviewTitle,
    const char *reviewSubTitle, /* Most often this is empty, but sometimes indicates a path / index */
    const char *finish_page_text, /*unused on Nano*/
    nbgl_choiceCallback_t choice_callback)
{
#ifndef TARGET_NANOS
    review_tagValueList = tagValueList;
    review_icon = icon;
    review_finish_page_text = finish_page_text;
    review_choice_callback = choice_callback;

    nbgl_useCaseReviewStart(icon,
                            reviewTitle,
                            reviewSubTitle,
                            "Reject transaction",
                            tx_review_continue,
                            tx_ask_transaction_rejection_confirmation);
#else
    UNUSED(reviewSubTitle);
    UNUSED(finish_page_text);
    nbgl_useCaseStaticReview(tagValueList, icon, reviewTitle, "Approve", "Reject", choice_callback);
#endif

}

#ifndef TARGET_NANOS
static void addr_review_continue(void) {
    nbgl_useCaseAddressConfirmation(review_address, review_choice_callback);
}

static void addr_review_rejection(void) {
    review_choice_callback(false);
}
#endif

void nbgl_useCaseAddressReview(
    const char *address,
    const nbgl_icon_details_t *icon,
    const char *reviewTitle,
    const char *reviewSubTitle,
    nbgl_choiceCallback_t choice_callback)
{
#ifndef TARGET_NANOS
    review_address = address;
    review_choice_callback = choice_callback;

    nbgl_useCaseReviewStart(icon,
                            reviewTitle,
                            reviewSubTitle,
                            "Cancel",
                            addr_review_continue,
                            addr_review_rejection);
#else
    UNUSED(reviewSubTitle);
    nbgl_useCaseAddressConfirmation(icon, reviewTitle, address, choice_callback);
#endif
}
