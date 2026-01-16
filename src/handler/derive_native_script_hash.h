#include "bip44.h"
#include "cardano_constants.h"
#include "deriveNativeScriptHash_types.h"
/*uint16_t deriveNativeScriptHash_handleAPDU(uint8_t p1,
                                           uint8_t p2,
                                           const uint8_t* wireDataBuffer,
                                           size_t wireDataSize,
                                           bool isNewCall);*/
//TODO: comments
/**
 * Handler for INS_DERIVE_NATIVE_SCRIPT_HASH command. Send APDU response with ASCII
 * encoded name of the application.
 *
 * @see variable APPNAME in Makefile.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
void handler_derive_native_script_hash(const buffer_t *cdata, uint8_t script_type);

/*
// a special type for distinguishing what to show in the UI, makes it easier
// to handle PUBKEY DEVICE_OWNED vs THIRD_PARTY
typedef enum {
    UI_SCRIPT_PUBKEY_PATH = 0,  // aka DEVICE_OWNED
    UI_SCRIPT_PUBKEY_HASH,      // aka THIRD_PARTY
    UI_SCRIPT_ALL,
    UI_SCRIPT_ANY,
    UI_SCRIPT_N_OF_K,
    UI_SCRIPT_INVALID_BEFORE,
    UI_SCRIPT_INVALID_HEREAFTER,
} ui_native_script_type;

typedef struct {
    uint32_t totalScripts;
    uint32_t remainingScripts;
} complex_native_script_t;

typedef union {
    uint32_t requiredScripts;
    bip44_path_t pubkeyPath;
    uint8_t pubkeyHash[ADDRESS_KEY_HASH_LENGTH];
    uint64_t timelock;
} native_script_content_t;*/