#pragma once

#include <stdbool.h>

#include "glyphs.h"

typedef enum {
    STATUS_TYPE_TRANSACTION_SIGNED = 0,
    STATUS_TYPE_TRANSACTION_REJECTED = 1,
} nbgl_reviewStatusType_t;

typedef struct {
    const char *item;
    const char *value;
} nbgl_contentTagValue_t;

typedef struct {
    uint8_t nbPairs;
    nbgl_contentTagValue_t *pairs;
} nbgl_contentTagValueList_t;

typedef enum {
    CENTERED_INFO_WARNING = 0,
    BAR_LIST_WARNING = 1,
} nbgl_warning_type_e;

typedef struct {
    const nbgl_icon_details_t *icon;
    const char *title;
    const char *description;
} nbgl_centered_info_t;

typedef struct nbgl_warningDetails_s {
    const char *title;
    nbgl_warning_type_e type;
    nbgl_centered_info_t centeredInfo;
    struct {
        size_t nbBars;
        const nbgl_icon_details_t **icons;
        const char **texts;
        const char **subTexts;
        struct nbgl_warningDetails_s *details;
    } barList;
} nbgl_warningDetails_t;

typedef struct {
    const char *title;
    const nbgl_icon_details_t *icon;
    const char *description;
} nbgl_contentCenter_t;

typedef struct {
    nbgl_warningDetails_t *introDetails;
    nbgl_warningDetails_t *reviewDetails;
    nbgl_contentCenter_t *info;
    const nbgl_icon_details_t *introTopRightIcon;
    const nbgl_icon_details_t *reviewTopRightIcon;
} nbgl_warning_t;

void nbgl_useCaseSpinner(const char *text);
void nbgl_useCaseStatus(const char *text, bool success, void (*callback)(void));
void nbgl_useCaseReviewStatus(nbgl_reviewStatusType_t status, void (*callback)(void));
