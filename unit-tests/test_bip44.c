#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "addressUtils/bip44.h"

#define HD HARDENED_BIP32  // 0x80000000

// Helper to initialize BIP44 path
static void pathSpec_init(bip44_path_t* pathSpec, const uint32_t* pathArray, uint32_t pathLength) {
    pathSpec->length = pathLength;
    if (pathLength > 0) {
        memmove(pathSpec->path, pathArray, pathLength * 4);
    }
}

// Test case: simple numeric paths without hardening
static void test_bip44_simple_paths(void **state) {
    (void) state;

    struct {
        const uint32_t* path;
        size_t path_len;
        const char* expected;
    } testVectors[] = {
        // Simple paths without hardening
        {(uint32_t[]){1, 2, 3, 4, 5}, 5, "m/1/2/3/4/5"},
        {(uint32_t[]){0}, 1, "m/0"},
        {(uint32_t[]){1}, 1, "m/1"},
        {(uint32_t[]){44, 1815, 0}, 3, "m/44/1815/0"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        bip44_path_t pathSpec;
        pathSpec_init(&pathSpec, testVectors[i].path, testVectors[i].path_len);

        char result[256] = {0};
        bool success = format_bip44_path(&pathSpec, result, sizeof(result));
        assert_true(success);
        size_t resultLen = strlen(result);
        // Result length should be expected string length (without null terminator)
        assert_int_equal(resultLen, strlen(testVectors[i].expected));
        // String content should match
        assert_string_equal(result, testVectors[i].expected);
    }
}

// Test case: hardened paths (with ')
static void test_bip44_hardened_paths(void **state) {
    (void) state;

    struct {
        const uint32_t* path;
        size_t path_len;
        const char* expected;
    } testVectors[] = {
        // Hardened paths (with ')
        {(uint32_t[]){HD + 44, HD + 1815, HD + 0, 1, 55}, 5, "m/44'/1815'/0'/1/55"},
        {(uint32_t[]){HD + 44, HD + 1815}, 2, "m/44'/1815'"},
        {(uint32_t[]){HD + 0}, 1, "m/0'"},
        // Mixed paths
        {(uint32_t[]){HD + 44, HD + 1815, HD + 0, 0, 0}, 5, "m/44'/1815'/0'/0/0"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        bip44_path_t pathSpec;
        pathSpec_init(&pathSpec, testVectors[i].path, testVectors[i].path_len);

        char result[256] = {0};
        bool success = format_bip44_path(&pathSpec, result, sizeof(result));
        assert_true(success);
        size_t resultLen = strlen(result);
        assert_int_equal(resultLen, strlen(testVectors[i].expected));
        assert_string_equal(result, testVectors[i].expected);
    }
}

// Test case: empty path (root)
static void test_bip44_root_path(void **state) {
    (void) state;

    bip44_path_t pathSpec;
    pathSpec.length = 0;

    char result[256] = {0};
    bool success = format_bip44_path(&pathSpec, result, sizeof(result));
    assert_true(success);
    size_t resultLen = strlen(result);

    // Root path should be just "m"
    assert_int_equal(resultLen, 1);
    assert_string_equal(result, "m");
}

// Test case: Cardano standard derivation paths
static void test_bip44_cardano_paths(void **state) {
    (void) state;

    struct {
        const uint32_t* path;
        size_t path_len;
        const char* expected;
        const char* description;
    } testVectors[] = {
        // Cardano standard paths (CIP-3)
        {(uint32_t[]){HD + 1852, HD + 1815, HD + 0, 0, 0}, 5,
         "m/1852'/1815'/0'/0/0",
         "Shelley payment address path"},

        {(uint32_t[]){HD + 1852, HD + 1815, HD + 0, 2, 0}, 5,
         "m/1852'/1815'/0'/2/0",
         "Shelley stake key path"},

        {(uint32_t[]){HD + 1852, HD + 1815, HD + 0}, 3,
         "m/1852'/1815'/0'",
         "Shelley account path"},

        {(uint32_t[]){HD + 1852, HD + 1815}, 2,
         "m/1852'/1815'",
         "Shelley coin path"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        bip44_path_t pathSpec;
        pathSpec_init(&pathSpec, testVectors[i].path, testVectors[i].path_len);

        char result[256] = {0};
        bool success = format_bip44_path(&pathSpec, result, sizeof(result));
        assert_true(success);
        size_t resultLen = strlen(result);
        assert_int_equal(resultLen, strlen(testVectors[i].expected));
        assert_string_equal(result, testVectors[i].expected);
    }
}

// Test case: buffer size constraints
static void test_bip44_buffer_size(void **state) {
    (void) state;

    bip44_path_t pathSpec;
    uint32_t path[] = {HD + 44, HD + 1815, HD + 0, 0, 0};
    pathSpec_init(&pathSpec, path, 5);

    char result[256] = {0};

    // Test with proper minimum buffer size (MAX_BIP44_PATH_STRING_LENGTH + 1)
    // The function requires at least this much space
    const char* expected = "m/44'/1815'/0'/0/0";
    bool success = format_bip44_path(&pathSpec, result, 256);
    assert_true(success);
    size_t resultLen = strlen(result);
    assert_int_equal(resultLen, strlen(expected));
    assert_string_equal(result, expected);

    // Test with larger buffer
    memset(result, 0, sizeof(result));
    success = format_bip44_path(&pathSpec, result, sizeof(result));
    assert_true(success);
    resultLen = strlen(result);
    assert_int_equal(resultLen, strlen(expected));
    assert_string_equal(result, expected);
}

// Test case: maximum depth paths
static void test_bip44_max_depth_paths(void **state) {
    (void) state;

    // Maximum BIP44 path depth is 5 (m/purpose'/coin'/account'/change/address_index)
    uint32_t maxDepthPath[] = {HD + 44, HD + 1815, HD + 0, 0, 0};
    bip44_path_t pathSpec;
    pathSpec_init(&pathSpec, maxDepthPath, 5);

    char result[256] = {0};
    bool success = format_bip44_path(&pathSpec, result, sizeof(result));
    assert_true(success);
    size_t resultLen = strlen(result);
    assert_int_equal(resultLen, strlen("m/44'/1815'/0'/0/0"));
    assert_string_equal(result, "m/44'/1815'/0'/0/0");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bip44_simple_paths),
        cmocka_unit_test(test_bip44_hardened_paths),
        cmocka_unit_test(test_bip44_root_path),
        cmocka_unit_test(test_bip44_cardano_paths),
        cmocka_unit_test(test_bip44_buffer_size),
        cmocka_unit_test(test_bip44_max_depth_paths),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
