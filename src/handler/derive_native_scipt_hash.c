#include "utils/utils.h"
#include "buffer.h"
#include "derive_native_script_hash.h"
#include "globals.h"
#include "securityPolicy.h"
#include "utils/assert.h"
#include "nbgl_use_case.h"

#include "io.h"

#include "ux.h"
#include "utils.h"
#include "utils/cardano_os_utils.h"
#include "display.h"
#include "deriveNativeScriptHash/deriveNativeScriptHash_types.h"
#include "deriveNativeScriptHash/derive_native_script_hash_builder.h"
#include "cbor.h"
#include "bech32.h"
#include "bip44.h"
#include "buffer_utils.h"

// Complex native script handlers
static int deriveNativeScriptHash_handleAll() {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    // TODO: add return and check?
    nativeScriptHashBuilder_startComplexScript_all(
        &ctx->hashBuilder,
        ctx->complexScripts[ctx->level].remainingScripts);
    ctx->ui_step = UI_SCRIPT_ALL;
    security_policy_t policy = POLICY_SHOW;
    ui_display_native_script_hash(policy);
    return 0;
}

static int deriveNativeScriptHash_handleAny() {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    // TODO: add return and check?
    nativeScriptHashBuilder_startComplexScript_any(
        &ctx->hashBuilder,
        ctx->complexScripts[ctx->level].remainingScripts);
    ctx->ui_step = UI_SCRIPT_ANY;
    security_policy_t policy = POLICY_SHOW;
    ui_display_native_script_hash(policy);
    return 0;
}

static int deriveNativeScriptHash_handleNofK(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    bool read32 = buffer_read_u32(cdata, &ctx->scriptContent.requiredScripts, BE);
    if (read32 == false) {
        // TODO: should return message response?
        return -1;
    }
    if (ctx->complexScripts[ctx->level].remainingScripts < ctx->scriptContent.requiredScripts) {
        // TODO: should return message response?
        return -1;
    }
    // TODO: add return and check?
    nativeScriptHashBuilder_startComplexScript_n_of_k(
        &ctx->hashBuilder,
        ctx->scriptContent.requiredScripts,
        ctx->complexScripts[ctx->level].remainingScripts);

    ctx->ui_step = UI_SCRIPT_N_OF_K;
    security_policy_t policy = POLICY_SHOW;
    ui_display_native_script_hash(policy);
    return 0;
}

static inline bool isComplexScriptFinished() {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    return ctx->level > 0 && ctx->complexScripts[ctx->level].remainingScripts == 0;
}

static inline int complexScriptFinished() {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    while (isComplexScriptFinished()) {
        ASSERT(ctx->level > 0);
        ctx->level--;
        ASSERT(ctx->level < MAX_SCRIPT_DEPTH);
        ASSERT(ctx->complexScripts[ctx->level].remainingScripts > 0);
        ctx->complexScripts[ctx->level].remainingScripts--;
    }
    return 0;
}

static inline int simpleScriptFinished() {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    ASSERT(ctx->level < MAX_SCRIPT_DEPTH);
    ASSERT(ctx->complexScripts[ctx->level].remainingScripts > 0);
    ctx->complexScripts[ctx->level].remainingScripts--;
    if (isComplexScriptFinished()) {
        complexScriptFinished();
    }
    return 0;
}

static inline bool areMoreScriptsExpected() {
    // if the number of remaining scripts is not bigger than 0, then this request
    // is invalid in the current context, as Ledger was not expecting another
    // script to be parsed
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    if (ctx->level >= MAX_SCRIPT_DEPTH) {
        return false;
    }
    return ctx->complexScripts[ctx->level].remainingScripts > 0;
}

// Simple native script handlers
static int deriveNativeScriptHash_handleDeviceOwnedPubkey(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    bool read_path = buffer_read_bip44_path(cdata, &ctx->scriptContent.pubkeyPath);
    if (!read_path) {
        return -1;
    }
    uint8_t pubkeyHash[ADDRESS_KEY_HASH_LENGTH] = {0};
    bip44_pathToKeyHash(&ctx->scriptContent.pubkeyPath, pubkeyHash, ADDRESS_KEY_HASH_LENGTH);

    // TODO: add return and check?
    nativeScriptHashBuilder_addScript_pubkey(&ctx->hashBuilder, pubkeyHash, SIZEOF(pubkeyHash));
    ctx->ui_step = UI_SCRIPT_PUBKEY_PATH;
    security_policy_t policy = policyForDeriveNativeScriptHashDevicePubkey(&ctx->scriptContent.pubkeyPath);
    ui_display_native_script_hash(policy);
    return 0;
}

static int deriveNativeScriptHash_handleThirdPartyPubkey(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    STATIC_ASSERT(SIZEOF(ctx->scriptContent.pubkeyHash) == ADDRESS_KEY_HASH_LENGTH,
                  "incorrect key hash size in script");
    bool read_bytes =
        buffer_read_bytes(cdata, ctx->scriptContent.pubkeyHash, ADDRESS_KEY_HASH_LENGTH);
    if (!read_bytes) {
        return -1;
    }
    // TODO: add return and check?
    nativeScriptHashBuilder_addScript_pubkey(&ctx->hashBuilder,
                                             ctx->scriptContent.pubkeyHash,
                                             SIZEOF(ctx->scriptContent.pubkeyHash));
    ctx->ui_step = UI_SCRIPT_PUBKEY_HASH;
    security_policy_t policy = POLICY_SHOW;
    ui_display_native_script_hash(policy);
    return 0;
}

static int deriveNativeScriptHash_handlePubkey(buffer_t *cdata) {
    uint8_t pubkeyType = 0;
    bool read_pubkey = buffer_read_u8(cdata, &pubkeyType);
    if (!read_pubkey) {
        return -1;
    }
    switch (pubkeyType) {
        case KEY_REFERENCE_PATH:
            return deriveNativeScriptHash_handleDeviceOwnedPubkey(cdata);
        case KEY_REFERENCE_HASH:
            return deriveNativeScriptHash_handleThirdPartyPubkey(cdata);
        default:
            return -1;
    }
    return 0;
}

static int deriveNativeScriptHash_handleInvalidBefore(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    bool read_timelock = buffer_read_u64(cdata, &ctx->scriptContent.timelock, BE);
    if (!read_timelock) {
        return -1;
    }
    // TODO: add return and check?
    nativeScriptHashBuilder_addScript_invalidBefore(&ctx->hashBuilder, ctx->scriptContent.timelock);
    ctx->ui_step = UI_SCRIPT_INVALID_BEFORE;
    security_policy_t policy = POLICY_SHOW;
    ui_display_native_script_hash(policy);
    return 0;
}

static int deriveNativeScriptHash_handleInvalidHereafter(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    bool read_timelock = buffer_read_u64(cdata, &ctx->scriptContent.timelock, BE);
    if (!read_timelock) {
        return -1;
    }
    // TODO: add return and check?
    nativeScriptHashBuilder_addScript_invalidHereafter(&ctx->hashBuilder,
                                                       ctx->scriptContent.timelock);
    ctx->ui_step = UI_SCRIPT_INVALID_HEREAFTER;
    security_policy_t policy = POLICY_SHOW;
    ui_display_native_script_hash(policy);
    return 0;
}

// Finish native script handlers
int deriveNativeScriptHash_displayNativeScriptHash_bech32() {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    ctx->ui_step = UI_SCRIPT_DISPLAY_BECH32;
    security_policy_t policy = POLICY_SHOW;
    ui_display_native_script_hash(policy);
    return 0;
}

int deriveNativeScriptHash_displayNativeScriptHash_policyId() {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    ctx->ui_step = UI_SCRIPT_DISPLAY_POLICY_ID;
    security_policy_t policy = POLICY_SHOW;
    ui_display_native_script_hash(policy);
    return 0;
}

// Complex script start handler
static int deriveNativeScriptHash_handleComplexScriptStart(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;

    if (!areMoreScriptsExpected()) {
        // TODO: send error response?
        return -1;
    }

    ctx->level++;

    uint8_t nativeScriptType = 0;
    bool read_nativeScriptType = buffer_read_u8(cdata, &nativeScriptType);
    if (!read_nativeScriptType) {
        return -1;
    }

    bool read_remainingScripts =
        buffer_read_u32(cdata, &ctx->complexScripts[ctx->level].remainingScripts, BE);
    if (!read_remainingScripts) {
        return -1;
    }
    ctx->complexScripts[ctx->level].totalScripts = ctx->complexScripts[ctx->level].remainingScripts;

    int ret = 0;
    switch (nativeScriptType) {
        case NATIVE_SCRIPT_ALL:
            ret = deriveNativeScriptHash_handleAll();
            break;

        case NATIVE_SCRIPT_ANY:
            ret = deriveNativeScriptHash_handleAny();
            break;

        case NATIVE_SCRIPT_N_OF_K:
            ret = deriveNativeScriptHash_handleNofK(cdata);
            break;

        default:
            return -1;
    }

    if (ret == 0) {
        if (isComplexScriptFinished()) {
            return complexScriptFinished();
        }
    }
    return ret;
}

// Simple script handler
static int deriveNativeScriptHash_handleSimpleScript(buffer_t *cdata) {
    if (!areMoreScriptsExpected()) {
        // TODO: send error response?
        return -1;
    }

    uint8_t nativeScriptType = 0;
    bool read_nativeScriptType = buffer_read_u8(cdata, &nativeScriptType);
    if (!read_nativeScriptType) {
        return -1;
    }

    // parse data
    int ret = 0;
    switch (nativeScriptType) {
        case NATIVE_SCRIPT_PUBKEY:
            ret = deriveNativeScriptHash_handlePubkey(cdata);
            break;
        case NATIVE_SCRIPT_INVALID_BEFORE:
            ret = deriveNativeScriptHash_handleInvalidBefore(cdata);
            break;
        case NATIVE_SCRIPT_INVALID_HEREAFTER:
            ret = deriveNativeScriptHash_handleInvalidHereafter(cdata);
            break;
        default:
            return -1;
    }
    if (ret == 0) {
        return simpleScriptFinished();
    }
    return ret;
}

static int deriveNativeScriptHash_handleWholeNativeScriptFinish(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;

    // we finish only if there are no more scripts to be processed
    if (ctx->level != 0 || ctx->complexScripts[0].remainingScripts != 0) {
        // TODO: send error response?
        return -1;
    }

    if (ctx->level != 0 || ctx->complexScripts[0].remainingScripts != 0) {
        // TODO: send error response?
        return -1;
    }

    uint8_t displayFormat = 0;
    bool read_displayFormat = buffer_read_u8(cdata, &displayFormat);
    if (!read_displayFormat) {
        return -1;
    }
    int ret = 0;
    switch (displayFormat) {
        case DISPLAY_NATIVE_SCRIPT_HASH_BECH32: {
            // TODO: add return and check?
            nativeScriptHashBuilder_finalize(&ctx->hashBuilder,
                                             ctx->scriptHashBuffer,
                                             SCRIPT_HASH_LENGTH);

            ret = deriveNativeScriptHash_displayNativeScriptHash_bech32();
            break;
        }
        case DISPLAY_NATIVE_SCRIPT_HASH_POLICY_ID: {
            // TODO: add return and check?
            nativeScriptHashBuilder_finalize(&ctx->hashBuilder,
                                             ctx->scriptHashBuffer,
                                             SCRIPT_HASH_LENGTH);
            ret = deriveNativeScriptHash_displayNativeScriptHash_policyId();
            break;
        }
        default:
            return -1;
    }
    G_context.req_type = REQUEST_NONE;
    return ret;
}

int handler_derive_native_script_hash(buffer_t *cdata, uint8_t script_type) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;

    if (G_context.req_type != REQUEST_DERIVE_NATIVE_SCRIPT_HASH) {
        explicit_bzero(&G_context, sizeof(G_context));
        ctx->level = 0;
        ctx->complexScripts[ctx->level].remainingScripts = 1;
        nativeScriptHashBuilder_init(&ctx->hashBuilder);
        ctx->ui_step = UI_SCRIPT_INIT;
        security_policy_t policy = POLICY_SHOW;
        ui_display_native_script_hash(policy);
    }

    ctx->ui_step = UI_SCRIPT_CONTINUE;
    G_context.req_type = REQUEST_DERIVE_NATIVE_SCRIPT_HASH;

    int ret = 0;
    switch (script_type) {
        case STAGE_COMPLEX_SCRIPT_START:
            ret = deriveNativeScriptHash_handleComplexScriptStart(cdata);
            break;
        case STAGE_ADD_SIMPLE_SCRIPT:
            ret = deriveNativeScriptHash_handleSimpleScript(cdata);
            break;
        case STAGE_WHOLE_NATIVE_SCRIPT_FINISH:
            ret = deriveNativeScriptHash_handleWholeNativeScriptFinish(cdata);
            break;
        default:
            return -1;
            break;
    }
    return ret;
}
