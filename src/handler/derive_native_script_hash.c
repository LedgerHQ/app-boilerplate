#include "derive_native_script_hash.h"
#include "securityPolicy.h"
#include "apdu_constants.h"
#include "globals.h"
#include "buffer_utils.h"
#include "buffer.h"
#include "deriveNativeScriptHash/derive_native_script_hash_builder.h"
#include "ui_display_native_script_hash.h"

// static ins_derive_native_script_hash_context_t* ctx =
//     &(instructionState.deriveNativeScriptHashContext);

// // Helper functions

#define TRACE_WITH_CTX(message, ...)                    \
    TRACE(message "level = %u, remaining scripts = %u", \
          ##__VA_ARGS__,                                \
          ctx->level,                                   \
          ctx->complexScripts[ctx->level].remainingScripts)

static inline bool areMoreScriptsExpected() {
    // if the number of remaining scripts is not bigger than 0, then this request
    // is invalid in the current context, as Ledger was not expecting another
    // script to be parsed
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    ASSERT(ctx->level < MAX_SCRIPT_DEPTH);
    return ctx->complexScripts[ctx->level].remainingScripts > 0;
}

static inline bool isComplexScriptFinished() {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    ASSERT(ctx->level < MAX_SCRIPT_DEPTH);
    return ctx->level > 0 && ctx->complexScripts[ctx->level].remainingScripts == 0;
}

static inline void complexScriptFinished() {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    while (isComplexScriptFinished()) {
        ASSERT(ctx->level > 0);
        ctx->level--;

        ASSERT(ctx->level < MAX_SCRIPT_DEPTH);
        ASSERT(ctx->complexScripts[ctx->level].remainingScripts > 0);
        ctx->complexScripts[ctx->level].remainingScripts--;

        TRACE_WITH_CTX("complex script finished, ");
    }
}

static inline void simpleScriptFinished() {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    ASSERT(ctx->level < MAX_SCRIPT_DEPTH);
    ASSERT(ctx->complexScripts[ctx->level].remainingScripts > 0);
    ctx->complexScripts[ctx->level].remainingScripts--;

    TRACE_WITH_CTX("simple script finished, ");

    if (isComplexScriptFinished()) {
        complexScriptFinished();
    }
}

// // UI
// #define UI_DISPLAY_SCRIPT(UI_TYPE)               \
//     {                                            \
//         ctx->ui_scriptType = UI_TYPE;            \
//         ctx->ui_step = DISPLAY_UI_STEP_POSITION; \
//         deriveScriptHash_display_ui_runStep();   \
//     }

// // Start complex native script

static void deriveNativeScriptHash_handleAll(const buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    ASSERT(ctx->level < MAX_SCRIPT_DEPTH);
    TRACE("Ready to build ALL complex script");
    nativeScriptHashBuilder_startComplexScript_all(
        &ctx->hashBuilder,
        ctx->complexScripts[ctx->level].remainingScripts);
    TRACE("Ready to display ALL complex script");
    // UI_DISPLAY_SCRIPT(UI_SCRIPT_ALL);
}

static void deriveNativeScriptHash_handleAny(const buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;

    ASSERT(ctx->level < MAX_SCRIPT_DEPTH);
    nativeScriptHashBuilder_startComplexScript_any(
        &ctx->hashBuilder,
        ctx->complexScripts[ctx->level].remainingScripts);

    // UI_DISPLAY_SCRIPT(UI_SCRIPT_ANY);
}

static void deriveNativeScriptHash_handleNofK(const buffer_t *cdata) {
    // parse data
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    bool read32 = buffer_read_u32(cdata, &ctx->scriptContent.requiredScripts, BE);
    if (read32 == false) {
        // TODO: should return message response?
        return;
    }
    TRACE_WITH_CTX("required scripts = %u, ", ctx->scriptContent.requiredScripts);

    // validate that the received requiredScripts count makes sense
    ASSERT(ctx->level < MAX_SCRIPT_DEPTH);
    if (ctx->complexScripts[ctx->level].remainingScripts < ctx->scriptContent.requiredScripts) {
        // TODO: should return message response?
        return;
    }

    nativeScriptHashBuilder_startComplexScript_n_of_k(
        &ctx->hashBuilder,
        ctx->scriptContent.requiredScripts,
        ctx->complexScripts[ctx->level].remainingScripts);

    // UI_DISPLAY_SCRIPT(UI_SCRIPT_N_OF_K);
}

static void deriveNativeScriptHash_handleComplexScriptStart(const buffer_t *cdata) {
    // check if we can increase the level without breaking the MAX_SCRIPT_DEPTH constraint
    // VALIDATE(ctx->level + 1 < MAX_SCRIPT_DEPTH, ERR_INVALID_DATA);
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    if (!areMoreScriptsExpected()) {
        // TODO: send error response?
        return;
    }

    ctx->level++;

    uint8_t nativeScriptType = 0;
    bool read_nativeScriptType = buffer_read_u8(cdata, &nativeScriptType);
    if (!read_nativeScriptType) {
        return;
    }
    TRACE("native complex script type = %u", nativeScriptType);

    ASSERT(ctx->level < MAX_SCRIPT_DEPTH);
    bool read_remainingScripts =
        buffer_read_u32(cdata, &ctx->complexScripts[ctx->level].remainingScripts, BE);
    if (!read_remainingScripts) {
        return;
    }
    ctx->complexScripts[ctx->level].totalScripts = ctx->complexScripts[ctx->level].remainingScripts;

    // these handlers might read additional data from the view
    switch (nativeScriptType) {
#define CASE(TYPE, HANDLER) \
    case TYPE:              \
        HANDLER(cdata);     \
        break;
        CASE(NATIVE_SCRIPT_ALL, deriveNativeScriptHash_handleAll);
        CASE(NATIVE_SCRIPT_ANY, deriveNativeScriptHash_handleAny);
        CASE(NATIVE_SCRIPT_N_OF_K, deriveNativeScriptHash_handleNofK);
#undef CASE
        default:
            // TODO: send error response?
            return;
    }

    if (isComplexScriptFinished()) {
        complexScriptFinished();
    }
}

// // Simple native scripts

static void deriveNativeScriptHash_handleDeviceOwnedPubkey(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    bool read_path = buffer_read_bip44_path(cdata, &ctx->scriptContent.pubkeyPath);
    if (!read_path) {
        return;
    }
    uint8_t pubkeyHash[ADDRESS_KEY_HASH_LENGTH] = {0};
    bip44_pathToKeyHash(&ctx->scriptContent.pubkeyPath, pubkeyHash, ADDRESS_KEY_HASH_LENGTH);

    nativeScriptHashBuilder_addScript_pubkey(&ctx->hashBuilder, pubkeyHash, SIZEOF(pubkeyHash));

    // UI_DISPLAY_SCRIPT(UI_SCRIPT_PUBKEY_PATH);
}

static void deriveNativeScriptHash_handleThirdPartyPubkey(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    STATIC_ASSERT(SIZEOF(ctx->scriptContent.pubkeyHash) == ADDRESS_KEY_HASH_LENGTH,
                  "incorrect key hash size in script");
    bool read_bytes =
        buffer_read_bytes(cdata, ctx->scriptContent.pubkeyHash, ADDRESS_KEY_HASH_LENGTH);
    if (!read_bytes) {
        return;
    }
    nativeScriptHashBuilder_addScript_pubkey(&ctx->hashBuilder,
                                             ctx->scriptContent.pubkeyHash,
                                             SIZEOF(ctx->scriptContent.pubkeyHash));

    // UI_DISPLAY_SCRIPT(UI_SCRIPT_PUBKEY_HASH);
}

static void deriveNativeScriptHash_handlePubkey(buffer_t *cdata) {
    uint8_t pubkeyType = 0;
    bool read_pubkey = buffer_read_u8(cdata, &pubkeyType);
    if (!read_pubkey) {
        return;
    }
    TRACE("pubkey type = %u", pubkeyType);

    switch (pubkeyType) {
        case KEY_REFERENCE_PATH:
            deriveNativeScriptHash_handleDeviceOwnedPubkey(cdata);
            return;
        case KEY_REFERENCE_HASH:
            deriveNativeScriptHash_handleThirdPartyPubkey(cdata);
            return;
        // any other value for the pubkey type is invalid
        default:
            return;
    }
}

static void deriveNativeScriptHash_handleInvalidBefore(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    bool read_timelock = buffer_read_u64(cdata, &ctx->scriptContent.timelock, BE);
    if (!read_timelock) {
        return;
    }
    nativeScriptHashBuilder_addScript_invalidBefore(&ctx->hashBuilder, ctx->scriptContent.timelock);

    // UI_DISPLAY_SCRIPT(UI_SCRIPT_INVALID_BEFORE);
}

static void deriveNativeScriptHash_handleInvalidHereafter(buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;
    bool read_timelock = buffer_read_u64(cdata, &ctx->scriptContent.timelock, BE);
    if (!read_timelock) {
        return;
    }
    nativeScriptHashBuilder_addScript_invalidHereafter(&ctx->hashBuilder,
                                                       ctx->scriptContent.timelock);

    // UI_DISPLAY_SCRIPT(UI_SCRIPT_INVALID_HEREAFTER);
}

// #undef UI_DISPLAY_SCRIPT

static void deriveNativeScriptHash_handleSimpleScript(const buffer_t *cdata) {
    if (!areMoreScriptsExpected()) {
        // TODO: send error response?
        return;
    }

    uint8_t nativeScriptType = 0;
    bool read_nativeScriptType = buffer_read_u8(cdata, &nativeScriptType);
    if (!read_nativeScriptType) {
        return;
    }

    // parse data
    switch (nativeScriptType) {
#define CASE(TYPE, HANDLER) \
    case TYPE:              \
        HANDLER(cdata);     \
        break;
        CASE(NATIVE_SCRIPT_PUBKEY, deriveNativeScriptHash_handlePubkey);
        CASE(NATIVE_SCRIPT_INVALID_BEFORE, deriveNativeScriptHash_handleInvalidBefore);
        CASE(NATIVE_SCRIPT_INVALID_HEREAFTER, deriveNativeScriptHash_handleInvalidHereafter);
#undef CASE
        default:
            return;
    }

    simpleScriptFinished();
}

// // Whole native script finish
typedef enum {
    DISPLAY_NATIVE_SCRIPT_HASH_BECH32 = 1,
    DISPLAY_NATIVE_SCRIPT_HASH_POLICY_ID = 2,
} display_format;

static void deriveNativeScriptHash_handleWholeNativeScriptFinish(const buffer_t *cdata) {
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;

    // we finish only if there are no more scripts to be processed
    if (ctx->level != 0 || ctx->complexScripts[0].remainingScripts != 0) {
        // TODO: send error response?
        return;
    }

    uint8_t displayFormat = 0;
    bool read_displayFormat = buffer_read_u8(cdata, &displayFormat);
    if (!read_displayFormat) {
        return;
    }

        /*switch (displayFormat) {
    #define CASE(FORMAT, DISPLAY_FN)                                \
        case FORMAT:                                                \
            nativeScriptHashBuilder_finalize(&ctx->hashBuilder,     \
                                                ctx->scriptHashBuffer, \
                                                SCRIPT_HASH_LENGTH);   \
            DISPLAY_FN();                                           \
            break;
            CASE(DISPLAY_NATIVE_SCRIPT_HASH_BECH32,
                    deriveNativeScriptHash_displayNativeScriptHash_bech32);
            CASE(DISPLAY_NATIVE_SCRIPT_HASH_POLICY_ID,
                    deriveNativeScriptHash_displayNativeScriptHash_policyId);
    #undef CASE
            default:
                return;
        }*/
       return;
    }

typedef void subhandler_fn_t(const buffer_t *cdata);

enum {
    STAGE_COMPLEX_SCRIPT_START = 0x01,
    STAGE_ADD_SIMPLE_SCRIPT = 0x02,
    STAGE_WHOLE_NATIVE_SCRIPT_FINISH = 0x03,
};

typedef void subhandler_fn_t(const buffer_t *cdata);

static subhandler_fn_t *lookup_subhandler(uint8_t stript_type) {
    TRACE("lookup_subhandler for script type %u", stript_type);
    switch (stript_type) {
#define CASE(type, HANDLER) \
    case type:              \
        return HANDLER;
#define DEFAULT(HANDLER) \
    default:             \
        return HANDLER;
        CASE(STAGE_COMPLEX_SCRIPT_START, deriveNativeScriptHash_handleComplexScriptStart);
        CASE(STAGE_ADD_SIMPLE_SCRIPT, deriveNativeScriptHash_handleSimpleScript);
        CASE(STAGE_WHOLE_NATIVE_SCRIPT_FINISH, deriveNativeScriptHash_handleWholeNativeScriptFinish)
        DEFAULT(NULL);
#undef CASE
#undef DEFAULT
    }
}

void handler_derive_native_script_hash(const buffer_t *cdata, uint8_t script_type) {
    TRACE("handler_derive_native_script_hash");
    ins_derive_native_script_hash_ctx_t *ctx = &G_context.derive_native_script_hash_info;

    if (G_context.req_type != REQUEST_DERIVE_NATIVE_SCRIPT_HASH) {
        explicit_bzero(&G_context, sizeof(G_context));
        ctx->level = 0;
        ctx->complexScripts[ctx->level].remainingScripts = 1;
        nativeScriptHashBuilder_init(&ctx->hashBuilder);
        // ctx->ui_step = UI_SCRIPT_INIT;
        // security_policy_t policy = POLICY_SHOW;
        // ui_display_native_script_hash(policy);
    }

    subhandler_fn_t *subhandler = lookup_subhandler(script_type);
    // VALIDATE(subhandler != NULL, ERR_INVALID_REQUEST_PARAMETERS);
    subhandler(cdata);
    return;
}