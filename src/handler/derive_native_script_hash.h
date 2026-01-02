enum {
    STAGE_COMPLEX_SCRIPT_START,
    STAGE_ADD_SIMPLE_SCRIPT,
    STAGE_WHOLE_NATIVE_SCRIPT_FINISHED
};

/**
 * Handler for INS_DERIVE_NATIVE_SCRIPT_HASH command. Send APDU response with ASCII
 * encoded name of the application.
 *
 * @see variable APPNAME in Makefile.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
int handler_derive_native_script_hash(buffer_t *cdata, uint8_t script_type);