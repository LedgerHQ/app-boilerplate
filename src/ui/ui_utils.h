#pragma once

#include <stdbool.h>
#include <string.h>
#include "nbgl_use_case.h"
#include "utils/assert.h"
#include "ui_constants.h"
#include "addressUtils/bech32.h"

/**
 * UI formatting status - tracks result of UI string generation
 *
 * UI_STATUS_UNINITIALIZED (0): Default state after bzero - invalid to use, must call ui_reset_error_status()
 * UI_STATUS_SUCCESS (1):        All UI formatting succeeded
 * UI_STATUS_OUT_OF_MEMORY (2):  Memory allocation failure during UI formatting
 *
 * Note: Once set to error state, cannot be changed back to success (enforced by ui_set_error_status)
 */
typedef enum {
    UI_STATUS_UNINITIALIZED = 0,
    UI_STATUS_SUCCESS = 1,
    UI_STATUS_OUT_OF_MEMORY = 2,
} ui_status_t;

extern ui_status_t g_ui_error_status;

extern nbgl_contentTagValue_t *g_pairs;
extern nbgl_contentTagValueList_t *g_pairsList;

/**
 * Maximum number of UI pairs that can be displayed.
 * Also used as the limit for the allocation tracker.
 */
#define MAX_UI_PAIRS 250

/**
 * Initialize UI error status to SUCCESS before starting UI formatting
 * Must be called once at the beginning of UI string generation
 */
void ui_reset_error_status(void);

/**
 * Get final UI error status
 * Asserts if status was never initialized (still UNINITIALIZED)
 * @return UI_STATUS_SUCCESS or UI_STATUS_OUT_OF_MEMORY
 */
ui_status_t ui_get_error_status(void);

/**
 * Set UI error status
 * Cannot change from error state back to success (asserts if attempted)
 * @param status New status to set (must not be UNINITIALIZED)
 */
void ui_set_error_status(ui_status_t status);

bool ui_pairs_init(uint8_t nbPairs);
void ui_pairs_cleanup(void);
uint16_t ui_pairs_get_count(void);

// UI pair count verification macros for transaction formatting
#define START_COUNT() uint16_t _pairs_before = ui_pairs_get_count()
#define CHECK_COUNT(expected) \
    LEDGER_ASSERT(ui_pairs_get_count() - _pairs_before == (expected), \
                  "UI pairs mismatch: expected %d, actual %d", \
                  (expected), ui_pairs_get_count() - _pairs_before)

#ifdef __GNUC__
#define UI_STATIC_LABEL(label) ((void)sizeof(char[__builtin_constant_p(label) ? 1 : -1]), (label))
#else
#define UI_STATIC_LABEL(label) (label)
#endif

/**
 * Add a label-value pair to the UI pairs list with optional shrinking
 *
 * @param label static label string (should be constant, compile-time checked by UI_STATIC_LABEL macro)
 * @param tmp_buf temporary buffer containing the value (will be freed after use)
 * @param shrink if true, allocates exact size for the value; if false, uses buffer as-is
 * @return true on success, false on failure
 */
bool ui_pairs_add_static_label_impl(const char* label, char* tmp_buf, bool shrink);

/**
 * Add a label-value pair to the UI pairs list (legacy wrapper, always shrinks)
 * Use ui_pairs_add_static_label_impl with shrink=true for equivalent behavior
 *
 * @param label static label string (should be constant, compile-time checked by UI_STATIC_LABEL macro)
 * @param tmp_buf temporary buffer containing the value (will be freed after use)
 * @return true on success, false on failure
 */
bool ui_pairs_add_static_label(const char* label, char* tmp_buf);

/**
 * Track an allocated buffer for later cleanup
 *
 * @param ptr pointer to allocated buffer (can be NULL)
 */
void ui_track_allocation(void *ptr);

/**
 * Cleanup all tracked allocations and reset tracker
 */
void ui_cleanup_tracked_allocations(void);

/**
 * Allocate memory and automatically track it for cleanup
 * This ensures UI buffers are cleaned up even if allocated in loops
 * or on error paths where manual cleanup might be forgotten.
 *
 * @param size number of bytes to allocate
 * @return pointer to allocated memory, or NULL on failure
 */
void *ui_mem_alloc(size_t size);

/**
 * Format a single-parameter value and add to UI pairs.
 *
 * Allocates buffer, calls formatting function with standard 3-parameter signature
 * (value, output_buffer, buffer_size), verifies success and no truncation,
 * and adds result to UI pairs.
 *
 * Formatting function signature: bool format_fn(value_type value, char *out, size_t outSize)
 *
 * @param label      Static label for UI pair (use UI_STATIC_LABEL macro)
 * @param max_len    Maximum output string length (without null terminator)
 * @param format_fn  Formatting function with signature: bool fn(value, char*, size_t)
 * @param value      Value to format (passed as first argument to format_fn)
 */
#define UI_ADD_FORMAT1(label, max_len, format_fn, value) do { \
    char *_buf = (char *) app_mem_alloc((max_len) + UI_BUFFER_SAFETY_MARGIN); \
    if (_buf == NULL) { \
        ui_set_error_status(UI_STATUS_OUT_OF_MEMORY); \
        break; \
    } \
    bool _ok = format_fn((value), _buf, (max_len) + UI_BUFFER_SAFETY_MARGIN); \
    LEDGER_ASSERT(_ok, "Format failed: " #format_fn); \
    LEDGER_ASSERT(strlen(_buf) <= (max_len), "Buffer too short: " #format_fn " (max %u bytes)", (unsigned int)(max_len)); \
    if (!ui_pairs_add_static_label((label), _buf)) { \
        ui_set_error_status(UI_STATUS_OUT_OF_MEMORY); \
        break; \
    } \
} while(0)

/**
 * Format a two-parameter value and add to UI pairs.
 *
 * Allocates buffer, calls formatting function with signature
 * (param1, param2, output_buffer, buffer_size), verifies success and no truncation,
 * and adds result to UI pairs.
 *
 * Formatting function signature: bool format_fn(param1_type p1, param2_type p2, char *out, size_t outSize)
 *
 * @param label      Static label for UI pair (use UI_STATIC_LABEL macro)
 * @param max_len    Maximum output string length (without null terminator)
 * @param format_fn  Formatting function with signature: bool fn(p1, p2, char*, size_t)
 * @param param1     First parameter to pass to format_fn
 * @param param2     Second parameter to pass to format_fn
 */
#define UI_ADD_FORMAT2(label, max_len, format_fn, param1, param2) do { \
    char *_buf = (char *) app_mem_alloc((max_len) + UI_BUFFER_SAFETY_MARGIN); \
    if (_buf == NULL) { \
        ui_set_error_status(UI_STATUS_OUT_OF_MEMORY); \
        break; \
    } \
    bool _ok = format_fn((param1), (param2), _buf, (max_len) + UI_BUFFER_SAFETY_MARGIN); \
    LEDGER_ASSERT(_ok, "Format failed: " #format_fn); \
    LEDGER_ASSERT(strlen(_buf) <= (max_len), "Buffer too short: " #format_fn " (max %u bytes)", (unsigned int)(max_len)); \
    if (!ui_pairs_add_static_label((label), _buf)) { \
        ui_set_error_status(UI_STATUS_OUT_OF_MEMORY); \
        break; \
    } \
} while(0)

/**
 * Format a three-parameter value and add to UI pairs.
 *
 * Allocates buffer, calls formatting function with signature
 * (param1, param2, param3, output_buffer, buffer_size), verifies success and no truncation,
 * and adds result to UI pairs.
 *
 * Formatting function signature: bool format_fn(p1_type p1, p2_type p2, p3_type p3, char *out, size_t outSize)
 *
 * @param label      Static label for UI pair (use UI_STATIC_LABEL macro)
 * @param max_len    Maximum output string length (without null terminator)
 * @param format_fn  Formatting function with signature: bool fn(p1, p2, p3, char*, size_t)
 * @param param1     First parameter to pass to format_fn
 * @param param2     Second parameter to pass to format_fn
 * @param param3     Third parameter to pass to format_fn
 */
#define UI_ADD_FORMAT3(label, max_len, format_fn, param1, param2, param3) do { \
    char *_buf = (char *) app_mem_alloc((max_len) + UI_BUFFER_SAFETY_MARGIN); \
    if (_buf == NULL) { \
        ui_set_error_status(UI_STATUS_OUT_OF_MEMORY); \
        break; \
    } \
    bool _ok = format_fn((param1), (param2), (param3), _buf, (max_len) + UI_BUFFER_SAFETY_MARGIN); \
    LEDGER_ASSERT(_ok, "Format failed: " #format_fn); \
    LEDGER_ASSERT(strlen(_buf) <= (max_len), "Buffer too short: " #format_fn " (max %u bytes)", (unsigned int)(max_len)); \
    if (!ui_pairs_add_static_label((label), _buf)) { \
        ui_set_error_status(UI_STATUS_OUT_OF_MEMORY); \
        break; \
    } \
} while(0)

/**
 * Add a static string value directly to UI pairs without formatting.
 *
 * Directly adds a static constant string to the UI pairs list via ui_pairs_add_static_label_impl.
 * Use this when you have a simple constant string that doesn't need formatting.
 *
 * @param label Static label for UI pair (use UI_STATIC_LABEL macro)
 * @param value Static string value (use UI_STATIC_LABEL macro for compile-time validation)
 */
#define UI_ADD_STATIC(label, value) do { \
    if (!ui_pairs_add_static_label_impl((label), (char *)(value), false)) { \
        ui_set_error_status(UI_STATUS_OUT_OF_MEMORY); \
        break; \
    } \
} while(0)

/**
 * Format a four-parameter value and add to UI pairs.
 *
 * Allocates buffer, calls formatting function with signature
 * (param1, param2, param3, param4, output_buffer, buffer_size), verifies success and no truncation,
 * and adds result to UI pairs.
 *
 * Formatting function signature: bool format_fn(p1_type p1, p2_type p2, p3_type p3, p4_type p4, char *out, size_t outSize)
 *
 * @param label      Static label for UI pair (use UI_STATIC_LABEL macro)
 * @param max_len    Maximum output string length (without null terminator)
 * @param format_fn  Formatting function with signature: bool fn(p1, p2, p3, p4, char*, size_t)
 * @param param1     First parameter to pass to format_fn
 * @param param2     Second parameter to pass to format_fn
 * @param param3     Third parameter to pass to format_fn
 * @param param4     Fourth parameter to pass to format_fn
 */
#define UI_ADD_FORMAT4(label, max_len, format_fn, param1, param2, param3, param4) do { \
    char *_buf = (char *) app_mem_alloc((max_len) + UI_BUFFER_SAFETY_MARGIN); \
    if (_buf == NULL) { \
        ui_set_error_status(UI_STATUS_OUT_OF_MEMORY); \
        break; \
    } \
    bool _ok = format_fn((param1), (param2), (param3), (param4), _buf, (max_len) + UI_BUFFER_SAFETY_MARGIN); \
    LEDGER_ASSERT(_ok, "Format failed: " #format_fn); \
    LEDGER_ASSERT(strlen(_buf) <= (max_len), "Buffer too short: " #format_fn " (max %u bytes)", (unsigned int)(max_len)); \
    if (!ui_pairs_add_static_label((label), _buf)) { \
        ui_set_error_status(UI_STATUS_OUT_OF_MEMORY); \
        break; \
    } \
} while(0)
