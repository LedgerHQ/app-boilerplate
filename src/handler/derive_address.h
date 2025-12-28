#pragma once

static uint16_t RESPONSE_READY_MAGIC = 11223;

enum {
    RETURN_UI_STEP_WARNING = 100,
    RETURN_UI_STEP_BEGIN,
    RETURN_UI_STEP_PAYMENT_PATH,
    RETURN_UI_STEP_STAKING_INFO,
    RETURN_UI_STEP_CONFIRM,
    RETURN_UI_STEP_RESPOND,
    RETURN_UI_STEP_INVALID,
};

enum {
    DISPLAY_UI_STEP_WARNING = 200,
    DISPLAY_UI_STEP_PAYMENT_INFO,
    DISPLAY_UI_STEP_STAKING_INFO,
    DISPLAY_UI_STEP_ADDRESS,
    DISPLAY_UI_STEP_CONFIRM,
    DISPLAY_UI_STEP_RESPOND,
    DISPLAY_UI_STEP_INVALID
};


/**
 * Handler for INS_GET_APP_NAME command. Send APDU response with ASCII
 * encoded name of the application.
 *
 * @see variable APPNAME in Makefile.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
int handler_derive_address(buffer_t *cdata, uint8_t chunk_type);

