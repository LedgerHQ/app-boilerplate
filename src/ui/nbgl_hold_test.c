#include "nbgl_use_case.h"
#include "glyphs.h"


void ui_hold_test(void);

static nbgl_layoutTagValue_t pairs[2];
static nbgl_layoutTagValueList_t pairList;
static nbgl_pageInfoLongPress_t infoLongPress;

// called when long press button on 3rd page is long-touched or when reject footer is touched
static void review_choice(bool confirm) {
    if (confirm) {
        // display a status page and go back to main
        // validate_transaction(true);
        nbgl_useCaseStatus("TEST OK", true, ui_hold_test);
    } else {
        nbgl_useCaseStatus("TEST KO", true, ui_hold_test);
    }
}

void ui_hold_test(void) {
    // Setup data to display
    pairs[0].item = "Amount";
    pairs[0].value = "0";
    pairs[1].item = "Address";
    pairs[1].value = "0";

    // Setup list
    pairList.nbMaxLinesForValue = 0;
    pairList.nbPairs = 2;
    pairList.pairs = pairs;

    // Info long press
    infoLongPress.icon = &C_stax_app_boilerplate_64px;
    infoLongPress.text = "Sign transaction\nto send BOL";
    infoLongPress.longPressText = "Hold to sign";

    nbgl_useCaseStaticReview(&pairList, &infoLongPress, "Reject transaction", review_choice);
}