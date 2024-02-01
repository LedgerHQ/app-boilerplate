/**
 * @file nbgl_page.c
 * @brief Implementation of predefined pages management for Applications
 */

#ifdef NBGL_PAGE
/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include "nbgl_debug.h"
#include "nbgl_page.h"
#include "glyphs.h"
#include "os_pic.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void addContent(nbgl_pageContent_t *content, nbgl_layout_t *layout)
{
    if (content->title != NULL) {
        nbgl_layoutBar_t bar;
        bar.text      = content->title;
        bar.subText   = NULL;
        bar.iconRight = NULL;
        bar.iconLeft  = content->isTouchableTitle ? &C_leftArrow32px : NULL;
        bar.token     = content->titleToken;
        bar.centered  = true;
        bar.inactive  = false;
        bar.tuneId    = content->tuneId;
        nbgl_layoutAddTouchableBar(layout, &bar);
        nbgl_layoutAddSeparationLine(layout);
    }
    switch (content->type) {
        case INFO_LONG_PRESS: {
            nbgl_layoutCenteredInfo_t centeredInfo;
            centeredInfo.icon    = content->infoLongPress.icon;
            centeredInfo.text1   = content->infoLongPress.text;
            centeredInfo.text2   = NULL;
            centeredInfo.text3   = NULL;
            centeredInfo.style   = LARGE_CASE_INFO;
            centeredInfo.offsetY = -32;
            centeredInfo.onTop   = false;
            nbgl_layoutAddCenteredInfo(layout, &centeredInfo);
            nbgl_layoutAddLongPressButton(layout,
                                          content->infoLongPress.longPressText,
                                          content->infoLongPress.longPressToken,
                                          content->infoLongPress.tuneId);
            break;
        }
        case INFO_BUTTON: {
            nbgl_layoutCenteredInfo_t centeredInfo;
            nbgl_layoutButton_t       buttonInfo;

            centeredInfo.icon    = content->infoButton.icon;
            centeredInfo.text1   = content->infoButton.text;
            centeredInfo.text2   = NULL;
            centeredInfo.text3   = NULL;
            centeredInfo.style   = LARGE_CASE_INFO;
            centeredInfo.offsetY = -40;
            centeredInfo.onTop   = false;
            nbgl_layoutAddCenteredInfo(layout, &centeredInfo);

            buttonInfo.fittingContent = false;
            buttonInfo.icon           = NULL;
            buttonInfo.onBottom       = true;
            buttonInfo.style          = BLACK_BACKGROUND;
            buttonInfo.text           = content->infoButton.buttonText;
            buttonInfo.token          = content->infoButton.buttonToken;
            buttonInfo.tuneId         = content->infoButton.tuneId;
            nbgl_layoutAddButton(layout, &buttonInfo);
            break;
        }
        case CENTERED_INFO:
            nbgl_layoutAddCenteredInfo(layout, &content->centeredInfo);
            break;
        case TAG_VALUE_LIST:
            nbgl_layoutAddTagValueList(layout, &content->tagValueList);
            break;
        case TAG_VALUE_DETAILS: {
            uint16_t nbLines = nbgl_getTextNbLinesInWidth(
                content->tagValueDetails.tagValueList.smallCaseForValue ? SMALL_REGULAR_FONT
                                                                        : LARGE_MEDIUM_FONT,
                content->tagValueDetails.tagValueList.pairs[0].value,
                SCREEN_WIDTH - 2 * BORDER_MARGIN,
                content->tagValueDetails.tagValueList.wrapping);
            // automatically display a button if content is longer that nbMaxLinesForValue
            if (nbLines > (content->tagValueDetails.tagValueList.nbMaxLinesForValue)) {
                nbgl_layoutButton_t buttonInfo;
                content->tagValueDetails.tagValueList.nbMaxLinesForValue -= 3;
                nbgl_layoutAddTagValueList(layout, &content->tagValueDetails.tagValueList);
                buttonInfo.fittingContent = true;
                buttonInfo.icon           = content->tagValueDetails.detailsButtonIcon;
                buttonInfo.style          = WHITE_BACKGROUND;
                buttonInfo.text           = content->tagValueDetails.detailsButtonText;
                buttonInfo.token          = content->tagValueDetails.detailsButtonToken;
                buttonInfo.tuneId         = content->tagValueDetails.tuneId;
                buttonInfo.onBottom       = false;
                nbgl_layoutAddButton(layout, &buttonInfo);
            }
            else {
                nbgl_layoutAddTagValueList(layout, &content->tagValueDetails.tagValueList);
            }
            break;
        }
        case TAG_VALUE_CONFIRM: {
            nbgl_layoutButton_t buttonInfo;
            nbgl_layoutAddTagValueList(layout, &content->tagValueConfirm.tagValueList);
            if (content->tagValueConfirm.detailsButtonText != NULL) {
                buttonInfo.fittingContent = true;
                buttonInfo.icon           = content->tagValueConfirm.detailsButtonIcon;
                buttonInfo.style          = WHITE_BACKGROUND;
                buttonInfo.text           = content->tagValueConfirm.detailsButtonText;
                buttonInfo.token          = content->tagValueConfirm.detailsButtonToken;
                buttonInfo.tuneId         = content->tagValueConfirm.tuneId;
                buttonInfo.onBottom       = false;
                nbgl_layoutAddButton(layout, &buttonInfo);
            }
            if (content->tagValueConfirm.confirmationText != NULL) {
                buttonInfo.fittingContent = false;
                buttonInfo.icon           = NULL;
                buttonInfo.style          = BLACK_BACKGROUND;
                buttonInfo.text           = content->tagValueConfirm.confirmationText;
                buttonInfo.token          = content->tagValueConfirm.confirmationToken;
                buttonInfo.tuneId         = content->tagValueConfirm.tuneId;
                buttonInfo.onBottom       = true;
                nbgl_layoutAddButton(layout, &buttonInfo);
            }
            break;
        }
        case SWITCHES_LIST: {
            uint8_t i;
            for (i = 0; i < content->switchesList.nbSwitches; i++) {
                nbgl_layoutAddSwitch(layout, &content->switchesList.switches[i]);
                nbgl_layoutAddSeparationLine(layout);
            }
            break;
        }
        case INFOS_LIST: {
            uint8_t i;
            for (i = 0; i < content->infosList.nbInfos; i++) {
                nbgl_layoutAddText(
                    layout, content->infosList.infoTypes[i], content->infosList.infoContents[i]);
                nbgl_layoutAddSeparationLine(layout);
            }
            break;
        }
        case CHOICES_LIST:
            nbgl_layoutAddRadioChoice(layout, &content->choicesList);
            break;
        case BARS_LIST: {
            uint8_t i;
            for (i = 0; i < content->barsList.nbBars; i++) {
                nbgl_layoutBar_t bar;
                bar.text      = content->barsList.barTexts[i];
                bar.subText   = NULL;
                bar.iconRight = &C_Next32px;
                bar.iconLeft  = NULL;
                bar.token     = content->barsList.tokens[i];
                bar.centered  = false;
                bar.tuneId    = content->barsList.tuneId;
                nbgl_layoutAddTouchableBar(layout, &bar);
                nbgl_layoutAddSeparationLine(layout);
            }
            break;
        }
    }
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief draw a page with a centered text in large case, with a round check icon
 *
 * @param onActionCallback common callback for all actions on this page
 * @param ticker ticker configuration, set to NULL to disable it
 * @param text text in LEDGER style (UPPER CASE only)
 * @param tapActionToken token passed to onActionCallback, on a tap event
 * @return the page context (or NULL if error)
 */
nbgl_page_t *nbgl_pageDrawLedgerInfo(nbgl_layoutTouchCallback_t              onActionCallback,
                                     const nbgl_screenTickerConfiguration_t *ticker,
                                     const char                             *text,
                                     int                                     tapActionToken)
{
    nbgl_layoutDescription_t  layoutDescription;
    nbgl_layout_t            *layout;
    nbgl_layoutCenteredInfo_t centeredInfo = {.text1   = text,
                                              .text2   = NULL,
                                              .text3   = NULL,
                                              .style   = LARGE_CASE_INFO,
                                              .icon    = &C_round_check_64px,
                                              .offsetY = 0};

    layoutDescription.modal          = false;
    layoutDescription.withLeftBorder = true;

    layoutDescription.onActionCallback = onActionCallback;
    layoutDescription.tapActionText    = "";
    layoutDescription.tapActionToken   = tapActionToken;
    layoutDescription.tapTuneId        = TUNE_TAP_CASUAL;

    layoutDescription.ticker.tickerCallback  = ticker->tickerCallback;
    layoutDescription.ticker.tickerIntervale = ticker->tickerIntervale;
    layoutDescription.ticker.tickerValue     = ticker->tickerValue;
    layout                                   = nbgl_layoutGet(&layoutDescription);

    nbgl_layoutAddCenteredInfo(layout, &centeredInfo);

    nbgl_layoutDraw(layout);

    return (nbgl_page_t *) layout;
}

/**
 * @brief draw a spinner page with the given parameters. The spinner will "rotate" automatically
 * every 800 ms
 *
 * @param onActionCallback common callback for all actions on this page (unused, so set to NULL)
 * @param text text to use under spinner
 * @return the page context (or NULL if error)
 */
nbgl_page_t *nbgl_pageDrawSpinner(nbgl_layoutTouchCallback_t onActionCallback, const char *text)
{
    nbgl_layoutDescription_t layoutDescription;
    nbgl_layout_t           *layout;

    layoutDescription.modal          = false;
    layoutDescription.withLeftBorder = true;

    layoutDescription.onActionCallback = onActionCallback;
    layoutDescription.tapActionText    = NULL;

    layoutDescription.ticker.tickerCallback = NULL;
    layout                                  = nbgl_layoutGet(&layoutDescription);

    nbgl_layoutAddSpinner(layout, text, false);

    nbgl_layoutDraw(layout);

    return (nbgl_page_t *) layout;
}

/**
 * @brief draw a page with a centered info (icon and/or texts) with a touchable footer,
 * in a potential "tapable" area, with an optional top-right button, with an optional bottom button
 *
 * @param onActionCallback common callback for all actions on this page
 * @param ticker ticker configuration, set to NULL to disable it
 * @param info structure describing the centered info and other controls of this page
 * @return the page context (or NULL if error)
 */
nbgl_page_t *nbgl_pageDrawInfo(nbgl_layoutTouchCallback_t              onActionCallback,
                               const nbgl_screenTickerConfiguration_t *ticker,
                               const nbgl_pageInfoDescription_t       *info)
{
    nbgl_layoutDescription_t layoutDescription;
    nbgl_layout_t           *layout;

    layoutDescription.modal          = false;
    layoutDescription.withLeftBorder = true;

    layoutDescription.onActionCallback = onActionCallback;
    layoutDescription.tapActionText    = info->tapActionText;
    layoutDescription.tapActionToken   = info->tapActionToken;
    layoutDescription.tapTuneId        = info->tuneId;

    if (ticker != NULL) {
        layoutDescription.ticker.tickerCallback  = ticker->tickerCallback;
        layoutDescription.ticker.tickerIntervale = ticker->tickerIntervale;
        layoutDescription.ticker.tickerValue     = ticker->tickerValue;
    }
    else {
        layoutDescription.ticker.tickerCallback = NULL;
    }
    layout = nbgl_layoutGet(&layoutDescription);

    nbgl_layoutAddCenteredInfo(layout, &info->centeredInfo);

    // if action button but not QUIT_APP_TEXT bottom button, use a small black button
    if ((info->actionButtonText != NULL) && (info->bottomButtonStyle != QUIT_APP_TEXT)) {
        nbgl_layoutButton_t buttonInfo = {.fittingContent = true,
                                          .icon           = NULL,
                                          .onBottom       = false,
                                          .style          = BLACK_BACKGROUND,
                                          .text           = info->actionButtonText,
                                          .token          = info->bottomButtonsToken,
                                          .tuneId         = info->tuneId};
        nbgl_layoutAddButton(layout, &buttonInfo);
    }

    if (info->footerText != NULL) {
        nbgl_layoutAddFooter(layout, PIC(info->footerText), info->footerToken, info->tuneId);
    }
    if (info->topRightStyle != NO_BUTTON_STYLE) {
        const nbgl_icon_details_t *icon;
        if (info->topRightStyle == SETTINGS_ICON) {
            icon = &C_wheel32px;
        }
        else if (info->topRightStyle == INFO_ICON) {
            icon = &C_info_i_32px;
        }
        else if (info->topRightStyle == QUIT_ICON) {
            icon = &C_cross32px;
        }
        else {
            return NULL;
        }
        nbgl_layoutAddTopRightButton(layout, PIC(icon), info->topRightToken, info->tuneId);
    }
    if (info->bottomButtonStyle == QUIT_APP_TEXT) {
        // if action button and QUIT_APP_TEXT bottom button, use a pair of choice buttons
        if ((info->actionButtonText != NULL)) {
            nbgl_layoutChoiceButtons_t buttonsInfo = {.topText    = info->actionButtonText,
                                                      .bottomText = "Quit app",
                                                      .token      = info->bottomButtonsToken,
                                                      .style      = BOTH_ROUNDED_STYLE,
                                                      .tuneId     = info->tuneId};
            nbgl_layoutAddChoiceButtons(layout, &buttonsInfo);
        }
        else {
            nbgl_layoutButton_t buttonInfo = {.fittingContent = false,
                                              .icon           = NULL,
                                              .onBottom       = true,
                                              .style          = WHITE_BACKGROUND,
                                              .text           = "Quit app",
                                              .token          = info->bottomButtonsToken,
                                              .tuneId         = info->tuneId};
            nbgl_layoutAddButton(layout, &buttonInfo);
        }
    }
    else if (info->bottomButtonStyle != NO_BUTTON_STYLE) {
        const nbgl_icon_details_t *icon;
        if (info->bottomButtonStyle == SETTINGS_ICON) {
            icon = &C_wheel32px;
        }
        else if (info->bottomButtonStyle == INFO_ICON) {
            icon = &C_info_i_32px;
        }
        else if (info->bottomButtonStyle == QUIT_ICON) {
            icon = &C_cross32px;
        }
        else {
            return NULL;
        }
        nbgl_layoutAddBottomButton(
            layout, PIC(icon), info->bottomButtonsToken, false, info->tuneId);
    }
    nbgl_layoutDraw(layout);

    return (nbgl_page_t *) layout;
}

/**
 * @brief draw a confirmation page, with a centered info (icon and/or text), a button to confirm and
 * a footer to cancel
 *
 * @param onActionCallback common callback for all actions on this page
 * @param info structure describing the centered info and other controls of this page
 * @return the page context (or NULL if error)
 */
nbgl_page_t *nbgl_pageDrawConfirmation(nbgl_layoutTouchCallback_t                onActionCallback,
                                       const nbgl_pageConfirmationDescription_t *info)
{
    nbgl_layoutDescription_t layoutDescription;
    nbgl_layout_t           *layout;

    layoutDescription.modal          = info->modal;
    layoutDescription.withLeftBorder = true;

    layoutDescription.onActionCallback = onActionCallback;
    layoutDescription.tapActionText    = NULL;

    layoutDescription.ticker.tickerCallback = NULL;
    layout                                  = nbgl_layoutGet(&layoutDescription);
    if (info->cancelText == NULL) {
        nbgl_layoutButton_t buttonInfo = {.style          = BLACK_BACKGROUND,
                                          .text           = info->confirmationText,
                                          .icon           = NULL,
                                          .token          = info->confirmationToken,
                                          .fittingContent = false,
                                          .tuneId         = info->tuneId,
                                          .onBottom       = true};
        nbgl_layoutAddBottomButton(
            layout, PIC(&C_cross32px), info->cancelToken, true, info->tuneId);
        nbgl_layoutAddButton(layout, &buttonInfo);
    }
    else {
        nbgl_layoutChoiceButtons_t buttonsInfo = {.bottomText = PIC(info->cancelText),
                                                  .token      = info->confirmationToken,
                                                  .topText    = PIC(info->confirmationText),
                                                  .style      = ROUNDED_AND_FOOTER_STYLE,
                                                  .tuneId     = info->tuneId};
        nbgl_layoutAddChoiceButtons(layout, &buttonsInfo);
    }
    nbgl_layoutAddCenteredInfo(layout, &info->centeredInfo);

    nbgl_layoutDraw(layout);

    return (nbgl_page_t *) layout;
}

/**
 * @brief draw a generic content page, with the given content, and if nav parameter is not NULL,
 * with the given navigation controls (either with navigation bar or with "tap" and "back")
 *
 * @param onActionCallback common callback for all actions on this page
 * @param nav structure describing the navigation controls of this page (no navigation if NULL)
 * @param content structure describing the main content of this page
 * @param modal set to true to draw as a modal
 * @return the page context (or NULL if error)
 */
nbgl_page_t *nbgl_pageDrawGenericContentExt(nbgl_layoutTouchCallback_t       onActionCallback,
                                            const nbgl_pageNavigationInfo_t *nav,
                                            nbgl_pageContent_t              *content,
                                            bool                             modal)
{
    nbgl_layoutDescription_t layoutDescription;
    nbgl_layout_t           *layout;

    layoutDescription.modal                 = modal;
    layoutDescription.withLeftBorder        = true;
    layoutDescription.onActionCallback      = onActionCallback;
    layoutDescription.ticker.tickerCallback = NULL;

    if ((nav != NULL) && (nav->navType == NAV_WITH_TAP)) {
        layoutDescription.tapActionText  = nav->navWithTap.nextPageText;
        layoutDescription.tapActionToken = nav->navWithTap.nextPageToken;
        layoutDescription.tapTuneId      = nav->tuneId;
    }
    else {
        layoutDescription.tapActionText = NULL;
    }

    layout = nbgl_layoutGet(&layoutDescription);
    if (nav != NULL) {
        if (nav->navType == NAV_WITH_TAP) {
            if (nav->navWithTap.skipText == NULL) {
                nbgl_layoutAddFooter(layout, nav->navWithTap.quitText, nav->quitToken, nav->tuneId);
            }
            else {
                nbgl_layoutAddSplitFooter(layout,
                                          nav->navWithTap.quitText,
                                          nav->quitToken,
                                          nav->navWithTap.skipText,
                                          nav->navWithTap.skipToken,
                                          nav->tuneId);
            }
            if (nav->progressIndicator) {
                nbgl_layoutAddProgressIndicator(layout,
                                                nav->activePage,
                                                nav->nbPages,
                                                nav->navWithTap.backButton,
                                                nav->navWithTap.backToken,
                                                nav->tuneId);
            }
        }
        else if (nav->navType == NAV_WITH_BUTTONS) {
            nbgl_layoutNavigationBar_t navInfo = {.activePage  = nav->activePage,
                                                  .nbPages     = nav->nbPages,
                                                  .token       = nav->navWithButtons.navToken,
                                                  .withExitKey = nav->navWithButtons.quitButton,
                                                  .withSeparationLine = true,
                                                  .tuneId             = nav->tuneId};
            nbgl_layoutAddNavigationBar(layout, &navInfo);
            if (nav->progressIndicator) {
                nbgl_layoutAddProgressIndicator(
                    layout, nav->activePage, nav->nbPages, false, 0, nav->tuneId);
            }
        }
    }
    addContent(content, layout);
    nbgl_layoutDraw(layout);

    return (nbgl_page_t *) layout;
}

/**
 * @brief draw a generic content page, with the given content, and if nav parameter is not NULL,
 * with the given navigation controls (either with navigation bar or with "tap" and "back")
 *
 * @param onActionCallback common callback for all actions on this page
 * @param nav structure describing the navigation controls of this page (no navigation if NULL)
 * @param content structure describing the main content of this page
 * @return the page context (or NULL if error)
 */
nbgl_page_t *nbgl_pageDrawGenericContent(nbgl_layoutTouchCallback_t       onActionCallback,
                                         const nbgl_pageNavigationInfo_t *nav,
                                         nbgl_pageContent_t              *content)
{
    return nbgl_pageDrawGenericContentExt(onActionCallback, nav, content, false);
}

/**
 * @brief Release the page obtained with any of the nbgl_pageDrawXXX() functions
 *
 * @param page page to release
 * @return >= 0 if OK
 */
int nbgl_pageRelease(nbgl_page_t *page)
{
    int ret;

    LOG_DEBUG(PAGE_LOGGER, "nbgl_pageRelease(): \n");
    ret = nbgl_layoutRelease((nbgl_layout_t *) page);

    return ret;
}
#endif  // NBGL_PAGE
