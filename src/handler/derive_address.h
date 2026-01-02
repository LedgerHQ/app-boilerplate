#pragma once

static uint16_t RESPONSE_READY_MAGIC = 11223;

enum {
    RETURN_UI_STEP_WARNING = 100,
    RETURN_UI_STEP_BEGIN,
    RETURN_UI_STEP_RESPOND,
};

enum {
    DISPLAY_UI_STEP_WARNING = 200,
    DISPLAY_UI_STEP_BEGIN,
    DISPLAY_UI_STEP_RESPOND,
};

enum {
    P1_RETURN = 0x01,
    P1_DISPLAY = 0x02,
};

enum {
    RETURN_POLICY_DENY = -1,
    RETURN_BAD_PARSE = -2,
};


/**
 * Handler for INS_DERIVE_ADDRESS command. Send APDU response with ASCII
 * encoded name of the application.
 *
 * @see variable APPNAME in Makefile.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
int handler_derive_address(buffer_t *cdata, uint8_t display_type);

