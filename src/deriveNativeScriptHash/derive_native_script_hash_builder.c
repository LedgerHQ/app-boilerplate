#include "cbor.h"
#include "deriveNativeScriptHash/derive_native_script_hash_builder.h"
#include "cardano_constants.h"

#define TRACE_NATIVE_SCRIPT_HASH_BUILDER

#ifdef TRACE_NATIVE_SCRIPT_HASH_BUILDER
#define _TRACE(...)        TRACE(__VA_ARGS__)
#define _TRACE_BUFFER(...) TRACE_BUFFER(__VA_ARGS__)
#else
#define _TRACE(...)
#define _TRACE_BUFFER(...)
#endif  // TRACE_NATIVE_SCRIPT_HASH_BUILDER

#define APPEND_CBOR(type, value) \
    blake2b_224_append_cbor_data(&builder->nativeScriptHash, type, value)
#define APPEND_BUFFER(buffer, size) \
    blake2b_224_append_buffer_data(&builder->nativeScriptHash, buffer, size)

static void blake2b_224_append_buffer_data(blake2b_224_context_t* hashCtx,
                                           const uint8_t* buffer,
                                           size_t size) {
    _TRACE_BUFFER(buffer, size);
    blake2b_224_append(hashCtx, buffer, size);
}

__noinline_due_to_stack__ static void blake2b_224_append_cbor_data(blake2b_224_context_t* hashCtx,
                                                                   uint8_t type,
                                                                   uint64_t value) {
    uint8_t buffer[10] = {0};
    size_t size = 0;
    cbor_writeToken(type, value, buffer, SIZEOF(buffer), &size);
    _TRACE_BUFFER(buffer, size);
    blake2b_224_append(hashCtx, buffer, size);
}

static inline void advanceState(native_script_hash_builder_t* builder) {
    // advance state should be only called when state is not finished
    // the advance state determines the next state from the current level
    // and number of remaining scripts for the current level
    ASSERT(builder->state == NATIVE_SCRIPT_HASH_BUILDER_SCRIPT);

    _TRACE("Advancing state, level = %u, remaining scripts = %u",
           builder->level,
           builder->remainingScripts[builder->level]);

    if (builder->level == 0 && builder->remainingScripts[builder->level] == 0) {
        builder->state = NATIVE_SCRIPT_HASH_BUILDER_FINISHED;
    } else {
        builder->state = NATIVE_SCRIPT_HASH_BUILDER_SCRIPT;
    }
}

static inline bool isComplexScriptFinished(native_script_hash_builder_t* builder) {
    return builder->level > 0 && builder->remainingScripts[builder->level] == 0;
}

static void complexScriptFinished(native_script_hash_builder_t* builder) {
    while (isComplexScriptFinished(builder)) {
        ASSERT(builder->level > 0);
        builder->level--;

        ASSERT(builder->remainingScripts[builder->level] > 0);
        builder->remainingScripts[builder->level]--;
    }
}

static void simpleScriptFinished(native_script_hash_builder_t* builder) {
    ASSERT(builder->remainingScripts[builder->level] > 0);
    builder->remainingScripts[builder->level]--;

    if (isComplexScriptFinished(builder)) {
        complexScriptFinished(builder);
    }
}

void nativeScriptHashBuilder_init(native_script_hash_builder_t* builder) {
    TRACE("Serializing native script hash data");
    blake2b_224_init(&builder->nativeScriptHash);

    // the native script hash is computed as a CBOR representation of the script,
    // but with a zero byte prepended before the CBOR
    uint8_t init[1] = {0x00};
    APPEND_BUFFER(init, 1);

    builder->state = NATIVE_SCRIPT_HASH_BUILDER_SCRIPT;
    builder->level = 0;
    builder->remainingScripts[builder->level] = 1;
}

#define _DEFINE_COMPLEX_SCRIPT(name, type)                                                        \
    void nativeScriptHashBuilder_startComplexScript_##name(native_script_hash_builder_t* builder, \
                                                           uint32_t remainingScripts) {           \
        _TRACE("state = %d", builder->state);                                                     \
                                                                                                  \
        ASSERT(builder->state == NATIVE_SCRIPT_HASH_BUILDER_SCRIPT);                              \
                                                                                                  \
        /* Array(2)[ */                                                                           \
        /*    Unsigned[native script type], */                                                    \
        /*    Array(remainingScripts)[ */                                                         \
        /*       // entries added later */                                                        \
        /*    ], */                                                                               \
        /* ] */                                                                                   \
        APPEND_CBOR(CBOR_TYPE_ARRAY, 2);                                                          \
        APPEND_CBOR(CBOR_TYPE_UNSIGNED, type);                                                    \
        APPEND_CBOR(CBOR_TYPE_ARRAY, remainingScripts);                                           \
                                                                                                  \
        builder->level++;                                                                         \
        builder->remainingScripts[builder->level] = remainingScripts;                             \
        _TRACE("appended CBOR");                                                     \
                                                                                                  \
        if (isComplexScriptFinished(builder)) {                                                   \
            complexScriptFinished(builder);                                                       \
        }                                                                                         \
        advanceState(builder);                                                                    \
    }

_DEFINE_COMPLEX_SCRIPT(all, NATIVE_SCRIPT_ALL)
_DEFINE_COMPLEX_SCRIPT(any, NATIVE_SCRIPT_ANY)

#undef _DEFINE_COMPLEX_SCRIPT

void nativeScriptHashBuilder_startComplexScript_n_of_k(native_script_hash_builder_t* builder,
                                                       uint32_t requiredScripts,
                                                       uint32_t remainingScripts) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == NATIVE_SCRIPT_HASH_BUILDER_SCRIPT);

    // Array(3)[
    //    Unsigned[native script type = 3],
    //    Unsigned[requiredScripts],
    //    Array(remainingScripts)[
    //       // entries added later
    //    ],
    // ]
    APPEND_CBOR(CBOR_TYPE_ARRAY, 3);
    APPEND_CBOR(CBOR_TYPE_UNSIGNED, NATIVE_SCRIPT_N_OF_K);
    APPEND_CBOR(CBOR_TYPE_UNSIGNED, requiredScripts);
    APPEND_CBOR(CBOR_TYPE_ARRAY, remainingScripts);

    builder->level++;
    builder->remainingScripts[builder->level] = remainingScripts;

    if (isComplexScriptFinished(builder)) {
        complexScriptFinished(builder);
    }
    advanceState(builder);
}

void nativeScriptHashBuilder_addScript_pubkey(native_script_hash_builder_t* builder,
                                              const uint8_t* pubKeyHashBuffer,
                                              size_t pubKeyHashSize) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == NATIVE_SCRIPT_HASH_BUILDER_SCRIPT);
    ASSERT(pubKeyHashSize == ADDRESS_KEY_HASH_LENGTH);

    // Array(2)[
    //    Unsigned[native script type = 0],
    //    Bytes[pubKeyHash],
    // ]
    APPEND_CBOR(CBOR_TYPE_ARRAY, 2);
    APPEND_CBOR(CBOR_TYPE_UNSIGNED, NATIVE_SCRIPT_PUBKEY);
    APPEND_CBOR(CBOR_TYPE_BYTES, pubKeyHashSize);
    APPEND_BUFFER(pubKeyHashBuffer, pubKeyHashSize);

    simpleScriptFinished(builder);
    advanceState(builder);
}

#define _DEFINE_SIMPLE_TIMELOCK_SCRIPT(name, type)                                       \
    void nativeScriptHashBuilder_addScript_##name(native_script_hash_builder_t* builder, \
                                                  uint64_t timelock) {                   \
        _TRACE("state = %d", builder->state);                                            \
                                                                                         \
        ASSERT(builder->state == NATIVE_SCRIPT_HASH_BUILDER_SCRIPT);                     \
                                                                                         \
        /* Array(2)[ */                                                                  \
        /*    Unsigned[native script type], */                                           \
        /*    Unsigned[timelock], */                                                     \
        /* ] */                                                                          \
        APPEND_CBOR(CBOR_TYPE_ARRAY, 2);                                                 \
        APPEND_CBOR(CBOR_TYPE_UNSIGNED, type);                                           \
        APPEND_CBOR(CBOR_TYPE_UNSIGNED, timelock);                                       \
                                                                                         \
        simpleScriptFinished(builder);                                                   \
        advanceState(builder);                                                           \
    }

_DEFINE_SIMPLE_TIMELOCK_SCRIPT(invalidBefore, NATIVE_SCRIPT_INVALID_BEFORE);
_DEFINE_SIMPLE_TIMELOCK_SCRIPT(invalidHereafter, NATIVE_SCRIPT_INVALID_HEREAFTER);

#undef _DEFINE_SIMPLE_TIMELOCK_SCRIPT

void nativeScriptHashBuilder_finalize(native_script_hash_builder_t* builder,
                                      uint8_t* outBuffer,
                                      size_t outSize) {
    _TRACE("state = %d", builder->state);

    //ASSERT(builder->state == NATIVE_SCRIPT_HASH_BUILDER_FINISHED);

    ASSERT(outSize == SCRIPT_HASH_LENGTH);

    blake2b_224_finalize(&builder->nativeScriptHash, outBuffer, outSize);
}

#undef APPEND_BUFFER
#undef APPEND_CBOR
#undef _TRACE_BUFFER
#undef _TRACE