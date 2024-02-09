#include "nbgl_use_case.h"

typedef enum {
    NBGL_SYNC_RET_SUCCESS,
    NBGL_SYNC_RET_REJECTED,
    NBGL_SYNC_RET_RX_APDU,
    NBGL_SYNC_RET_ERROR
} sync_nbgl_ret_t;

sync_nbgl_ret_t sync_nbgl_useCaseTransactionReview(
    const nbgl_layoutTagValueList_t *tagValueList,
    const nbgl_icon_details_t *icon,
    const char *reviewTitle,
    const char *reviewSubTitle, /* Most often this is empty, but sometimes indicates a path / index */
    const char *finish_page_text /*unused on Nano*/);

sync_nbgl_ret_t sync_nbgl_useCaseAddressReview(
    const char *address,
    const nbgl_icon_details_t *icon,
    const char *reviewTitle,
    const char *reviewSubTitle);

sync_nbgl_ret_t sync_nbgl_useCaseStatus(const char *message, bool isSuccess);
