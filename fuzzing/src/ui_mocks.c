#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "os_helpers.h"
#include "nbgl_use_case.h"

// Mock icon definitions for fuzzing
const nbgl_icon_details_t C_cardano_64;
const nbgl_icon_details_t C_Warning_64px;
const nbgl_icon_details_t C_Warning_32px;
const nbgl_icon_details_t C_Info_Circle_64px;
const nbgl_icon_details_t C_Important_Circle_64px;

// Device-specific mock icons
const nbgl_icon_details_t C_icon_ada_stax;
const nbgl_icon_details_t C_icon_ada_flex;
const nbgl_icon_details_t C_icon_ada_nanox;
const nbgl_icon_details_t C_icon_ada_apex;
const nbgl_icon_details_t C_icon_warning;

void nbgl_useCaseHomeAndSettings(const char *appName,
                                 const nbgl_icon_details_t *appIcon,
                                 const char *tagline,
                                 const uint8_t initSettingPage,
                                 const nbgl_genericContents_t *settingContents,
                                 const nbgl_contentInfoList_t *infosList,
                                 const nbgl_homeAction_t *action,
                                 nbgl_callback_t quitCallback) {
    UNUSED(appName);
    UNUSED(appIcon);
    UNUSED(tagline);
    UNUSED(initSettingPage);
    UNUSED(settingContents);
    UNUSED(infosList);
    UNUSED(action);
    UNUSED(quitCallback);
}

void nbgl_useCaseChoice(const nbgl_icon_details_t *icon,
                        const char *message,
                        const char *subMessage,
                        const char *confirmText,
                        const char *rejectString,
                        nbgl_choiceCallback_t callback) {
    UNUSED(icon);
    UNUSED(message);
    UNUSED(subMessage);
    UNUSED(confirmText);
    UNUSED(rejectString);
    UNUSED(callback);
}

void nbgl_useCaseStatus(const char *message, bool isSuccess, nbgl_callback_t quitCallback) {
    UNUSED(message);
    UNUSED(isSuccess);
    UNUSED(quitCallback);
}

void nbgl_useCaseConfirm(const char *message,
                         const char *subMessage,
                         const char *confirmText,
                         const char *rejectText,
                         nbgl_callback_t callback) {
    UNUSED(message);
    UNUSED(subMessage);
    UNUSED(confirmText);
    UNUSED(rejectText);
    UNUSED(callback);
}

void nbgl_useCaseSpinner(const char *text) {
    UNUSED(text);
}

void nbgl_useCaseReviewStatus(nbgl_reviewStatusType_t reviewStatusType,
                              nbgl_callback_t quitCallback) {
    UNUSED(reviewStatusType);
    UNUSED(quitCallback);
}

void nbgl_useCaseReviewStart(const nbgl_icon_details_t *icon,
                             const char *reviewTitle,
                             const char *reviewSubTitle,
                             const char *rejectText,
                             nbgl_callback_t continueCallback,
                             nbgl_callback_t rejectCallback) {
    UNUSED(icon);
    UNUSED(reviewTitle);
    UNUSED(reviewSubTitle);
    UNUSED(rejectText);
    UNUSED(continueCallback);
    UNUSED(rejectCallback);
}

uint16_t nbgl_getTextNbLinesInWidth(nbgl_font_id_e fontId,
                                    const char *text,
                                    uint16_t maxWidth,
                                    bool wrapping) {
    UNUSED(fontId);
    UNUSED(text);
    UNUSED(maxWidth);
    UNUSED(wrapping);
    return 1;
}

void nbgl_useCaseAddressReview(const char *address,
                               const nbgl_contentTagValueList_t *additionalTagValueList,
                               const nbgl_icon_details_t *icon,
                               const char *reviewTitle,
                               const char *reviewSubTitle,
                               nbgl_choiceCallback_t choiceCallback) {
    UNUSED(address);
    UNUSED(additionalTagValueList);
    UNUSED(icon);
    UNUSED(reviewTitle);
    UNUSED(reviewSubTitle);
    UNUSED(choiceCallback);
}

nbgl_page_t *nbgl_pageDrawInfo(nbgl_layoutTouchCallback_t onActionCallback,
                               const nbgl_screenTickerConfiguration_t *ticker,
                               const nbgl_pageInfoDescription_t *info) {
    UNUSED(onActionCallback);
    UNUSED(ticker);
    UNUSED(info);
    return NULL;
}

nbgl_page_t *nbgl_pageDrawGenericContent(nbgl_layoutTouchCallback_t onActionCallback,
                                         const nbgl_pageNavigationInfo_t *nav,
                                         nbgl_pageContent_t *content) {
    UNUSED(onActionCallback);
    UNUSED(nav);
    UNUSED(content);
    return NULL;
}

int nbgl_pageRelease(nbgl_page_t *page) {
    UNUSED(page);
    return 0;
}

void nbgl_useCaseAdvancedReview(nbgl_operationType_t operationType,
                                const nbgl_contentTagValueList_t *tagValueList,
                                const nbgl_icon_details_t *icon,
                                const char *reviewTitle,
                                const char *reviewSubTitle,
                                const char *finishTitle,
                                const nbgl_tipBox_t *tipBox,
                                const nbgl_warning_t *warning,
                                nbgl_choiceCallback_t choiceCallback) {
    UNUSED(operationType);
    UNUSED(tagValueList);
    UNUSED(icon);
    UNUSED(reviewTitle);
    UNUSED(reviewSubTitle);
    UNUSED(finishTitle);
    UNUSED(tipBox);
    UNUSED(warning);
    UNUSED(choiceCallback);
}

void nbgl_useCaseReview(nbgl_operationType_t operationType,
                       const nbgl_contentTagValueList_t *tagValueList,
                       const nbgl_icon_details_t *icon,
                       const char *reviewTitle,
                       const char *reviewSubTitle,
                       const char *finishTitle,
                       nbgl_choiceCallback_t choiceCallback) {
    UNUSED(operationType);
    UNUSED(tagValueList);
    UNUSED(icon);
    UNUSED(reviewTitle);
    UNUSED(reviewSubTitle);
    UNUSED(finishTitle);
    UNUSED(choiceCallback);
}
