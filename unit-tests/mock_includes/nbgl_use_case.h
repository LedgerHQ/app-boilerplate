#pragma once

#include <stdbool.h>

typedef enum {
    STATUS_TYPE_TRANSACTION_SIGNED = 0,
    STATUS_TYPE_TRANSACTION_REJECTED = 1,
} nbgl_reviewStatusType_t;

void nbgl_useCaseSpinner(const char *text);
void nbgl_useCaseStatus(const char *text, bool success, void (*callback)(void));
void nbgl_useCaseReviewStatus(nbgl_reviewStatusType_t status, void (*callback)(void));
