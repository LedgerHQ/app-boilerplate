#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>

#include "globals.h"
#include "securityPolicy/securityPolicy.h"
#include "apdu/dispatcher.h"
#include "globals.h"

#include <cmocka.h>
#include "handler/derive_address.h"
#include "hexUtils.h"

#include "blake2b.h"

#define P1_RETURN  0x01
#define P1_DISPLAY 0x02

#define RETURN_POLICY_DENY -1
#define RETURN_BAD_PARSE -2

// ----------------------------------------------------------------------
// Simple mocks for IO and UI plumbing so we can drive the handler
// ----------------------------------------------------------------------

int io_send_response_buffers(const buffer_t *buffer_list, size_t buffer_count, uint16_t sw) {
    (void) buffer_list;
    (void) buffer_count;
    (void) sw;
    return 0;
}

int io_send_sw(uint16_t sw) {
    return io_send_response_buffers(NULL, 0, sw);
}

// ----------------------------------------------------------------------
// Test fixtures
// ----------------------------------------------------------------------

typedef struct {
    const char *name;
    uint8_t p1;
    const uint8_t *data;
    size_t data_len;
    int check_expected;
} derive_address_reject_fixture_t;

static const uint8_t PATH_TOO_SHORT[] = {
    BYRON, // address type
    0x2D, 0x96, 0x4A, 0x09, // mainnet byron prefix
    // "m/44'/1815'/1'" 
    0x03, 
    0x80, 0x00, 0x00, 0x2C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    NO_STAKING,
};

static const uint8_t PATH_INVALID_BYRON[] = {
    BYRON, // address type
    0x2D, 0x96, 0x4A, 0x09, // mainnet byron prefix
    // "m/44'/1815'/1'/5/10'"
    0x05, 
    0x80, 0x00, 0x00, 0x2C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x05,
    0x80, 0x00, 0x00, 0x0A,
    NO_STAKING,
};

static const uint8_t BYRON_PATH_SHELLEY[] = {
    BYRON, // address type
    0x2D, 0x96, 0x4A, 0x09, // mainnet byron prefix
    // "m/1852'/1815'/1'/0/10"
    0x05, 
    0x80, 0x00, 0x07, 0x3C,
    0x80, 0x00, 0x07, 0x17, 
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0A,
    NO_STAKING,
};

static const uint8_t BASE_KEY_WITH_BYRON[] = {
    BASE_PAYMENT_KEY_STAKE_KEY, // address type
    0x01, // mainnet shelley prefix
    // "m/1852'/1815'/1'/0/10"
    0x05, 
    0x80, 0x00, 0x00, 0x2C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    STAKING_KEY_PATH, // staking choice
    // staking path: m/1852'/1815'/1'/2/0
    0x05,
    0x80, 0x00, 0x07, 0x3C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00,
};

static const uint8_t BASE_KEY_WITH_WRONG_SPENDING_PATH[] = {
    BASE_PAYMENT_KEY_STAKE_KEY, // address type
    0x01, // mainnet shelley prefix
    // "m/1852'/1815'/1'/2/0",
    0x05, 
    0x80, 0x00, 0x07, 0x3C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00,
    STAKING_KEY_PATH, // staking choice
    // staking path: "m/1852'/1815'/1'/2/0"
    0x05,
    0x80, 0x00, 0x07, 0x3C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00,
};

static const uint8_t BASE_KEY_WITH_WRONG_STAKING_PATH_1[] = {
    BASE_PAYMENT_KEY_STAKE_KEY, // address type
    0x01, // mainnet shelley prefix
    // "m/1852'/1815'/1'/0/0"
    0x05,
    0x80, 0x00, 0x07, 0x3C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    STAKING_KEY_PATH, // staking choice
    // staking path: "m/1852'/1815'/1'/0/1"
    0x05,
    0x80, 0x00, 0x07, 0x3C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
};

static const uint8_t BASE_WITH_BYRON_SPENDING_PATH[] = {
    BASE_PAYMENT_KEY_STAKE_KEY, // address type
    0x01, // mainnet shelley prefix
    // "m/44'/1815'/1'/0/1"
    0x05,
    0x80, 0x00, 0x00, 0x2C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    STAKING_KEY_HASH, // staking choice
    // staking path: "m/1852'/1815'/1'/0/1"
    0x22, 0x2A, 0x94, 0x6B, 0x9A, 0xD3, 0xD2, 0xDD,
    0xF0, 0x29, 0xD3, 0xA8, 0x28, 0xF0, 0x46, 0x8A,
    0xEC, 0xE7, 0x68, 0x95, 0xF1, 0x5C, 0x9E, 0xFB,
    0xD6, 0x9B, 0x42, 0x77
};

static const uint8_t BASE_ADDRESS_NOT_ALLOWED[] = {
    BASE_PAYMENT_SCRIPT_STAKE_KEY, // address type
    0x01, // mainnet shelley prefix
    //"122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277",
    0x12, 0x2A, 0x94, 0x6B, 0x9A, 0xD3, 0xD2, 0xDD,
    0xF0, 0x29, 0xD3, 0xA8, 0x28, 0xF0, 0x46, 0x8A,
    0xEC, 0xE7, 0x68, 0x95, 0xF1, 0x5C, 0x9E, 0xFB,
    0xD6, 0x9B, 0x42, 0x77,
    STAKING_KEY_HASH,
    //"222a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277",
    0x22, 0x2A, 0x94, 0x6B, 0x9A, 0xD3, 0xD2, 0xDD,
    0xF0, 0x29, 0xD3, 0xA8, 0x28, 0xF0, 0x46, 0x8A,
    0xEC, 0xE7, 0x68, 0x95, 0xF1, 0x5C, 0x9E, 0xFB,
    0xD6, 0x9B, 0x42, 0x77
};

static const uint8_t POINTER_WITH_BYRON[] = {
    POINTER_KEY, // address type
    0x01, // mainnet shelley prefix
    0X05,
    // "m/44'/1815'/1'/0/0"
    0x80, 0x00, 0x00, 0x2C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    BLOCKCHAIN_POINTER,
    0x00, 0x00, 0x00, 0x01,  // blockIndex = 1
    0x00, 0x00, 0x00, 0x02,  // txIndex = 2
    0x00, 0x00, 0x00, 0x03,  // certificateIndex = 3
};

static const uint8_t POINTER_WRONG[] = {
    POINTER_KEY, // address type
    0x01, // mainnet shelley prefix
    0X05,
    // "m/44'/1815'/1'/0/0"
    0x80, 0x00, 0x00, 0x2C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    BLOCKCHAIN_POINTER,
    0x00, 0x00, 0x00, 0x01,  // blockIndex = 1
    0x00, 0x00, 0x00, 0x02,  // txIndex = 2
    0x00, 0x00, 0x00, 0x03,  // certificateIndex = 3
};

static const uint8_t ENTERPRISE_WITH_BYRON[] = {
    ENTERPRISE_KEY, // address type
    0x01, // mainnet shelley prefix
    0X05,
    // "m/44'/1815'/1'/0/0"
    0x80, 0x00, 0x00, 0x2C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    NO_STAKING
};

static const uint8_t ENTERPRISE_WITH_WRONG_PATH[] = {
    ENTERPRISE_KEY, // address type
    0x01, // mainnet shelley prefix
    0X05,
    // "m/1852'/1815'/1'/2/0",
    0x80, 0x00, 0x07, 0x3C,
    0x80, 0x00, 0x07, 0x17,
    0x80, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00,
    NO_STAKING
};

static const derive_address_reject_fixture_t DERIVE_ADDRESS_REJECT_FIXTURES[] = {
    {
        .name = "path too short",
        .p1 = P1_RETURN,
        .data = PATH_TOO_SHORT,
        .data_len = sizeof(PATH_TOO_SHORT),
        .check_expected = RETURN_POLICY_DENY,
    },
    {
        .name = "invalid path",
        .p1 = P1_RETURN,
        .data = PATH_INVALID_BYRON,
        .data_len = sizeof(PATH_INVALID_BYRON),
        .check_expected = RETURN_POLICY_DENY,
    },
    {
        .name = "Byron with Shelley path",
        .p1 = P1_RETURN,
        .data = BYRON_PATH_SHELLEY,
        .data_len = sizeof(BYRON_PATH_SHELLEY),
        .check_expected = RETURN_POLICY_DENY,
    },
    {
        .name = "base key/key with Byron spending path",
        .p1 = P1_RETURN,
        .data = BASE_KEY_WITH_BYRON,
        .data_len = sizeof(BASE_KEY_WITH_BYRON),
        .check_expected = RETURN_POLICY_DENY,
    },
    {
        .name = "base key/key with wrong spending path",
        .p1 = P1_RETURN,
        .data = BASE_KEY_WITH_WRONG_SPENDING_PATH,
        .data_len = sizeof(BASE_KEY_WITH_WRONG_SPENDING_PATH),
        .check_expected = RETURN_POLICY_DENY,
    },
    {
        .name = "base key/key with wrong staking path 1",
        .p1 = P1_RETURN,
        .data = BASE_KEY_WITH_WRONG_STAKING_PATH_1,
        .data_len = sizeof(BASE_KEY_WITH_WRONG_STAKING_PATH_1),
        .check_expected = RETURN_POLICY_DENY,
    },
    {
        .name = "base key/script with Byron spending path",
        .p1 = P1_RETURN,
        .data = BASE_WITH_BYRON_SPENDING_PATH,
        .data_len = sizeof(BASE_WITH_BYRON_SPENDING_PATH),
        .check_expected = RETURN_POLICY_DENY,
    },
    {
        .name = "base address scripthash/keyhash not allowed",
        .p1 = P1_RETURN,
        .data = BASE_ADDRESS_NOT_ALLOWED,
        .data_len = sizeof(BASE_ADDRESS_NOT_ALLOWED),
        .check_expected = RETURN_POLICY_DENY,
    },
        {
        .name = "pointer with Byron spending path",
        .p1 = P1_RETURN,
        .data = POINTER_WITH_BYRON,
        .data_len = sizeof(POINTER_WITH_BYRON),
        .check_expected = RETURN_POLICY_DENY,
    },
        {
        .name = "pointer with wrong spending path",
        .p1 = P1_RETURN,
        .data = POINTER_WRONG,
        .data_len = sizeof(POINTER_WRONG),
        .check_expected = RETURN_POLICY_DENY,
    },
    {
        .name = "enterprise with Byron spending path",
        .p1 = P1_RETURN,
        .data = ENTERPRISE_WITH_BYRON,
        .data_len = sizeof(ENTERPRISE_WITH_BYRON),
        .check_expected = RETURN_POLICY_DENY,
    },
    {
        .name = "enterprise with wrong spending path",
        .p1 = P1_RETURN,
        .data = ENTERPRISE_WITH_WRONG_PATH,
        .data_len = sizeof(ENTERPRISE_WITH_WRONG_PATH),
        .check_expected = RETURN_POLICY_DENY,
    },
};


static void reset_context(void) {
    memset(&G_context, 0, sizeof(G_context));
}

void ui_deriveAddress_handleReturn(security_policy_t policy) {
    (void) policy;
}

void ui_deriveAddress_handleDisplay(security_policy_t policy) {
    (void) policy;
}

static void test_derive_address_reject_fixture(void **state) {
    const derive_address_reject_fixture_t *fixture = *state;

    reset_context();

    buffer_t buf = {
        .ptr = (uint8_t *)fixture->data,
        .size = fixture->data_len,
        .offset = 0,
    };

    int ret = handler_derive_address(&buf, fixture->p1);
    printf("Fixture %d\n", ret);
    assert_int_equal(ret, fixture->check_expected);
}

int main(void) {
    const size_t test_count = ARRAY_LEN(DERIVE_ADDRESS_REJECT_FIXTURES);
    struct CMUnitTest tests[ARRAY_LEN(DERIVE_ADDRESS_REJECT_FIXTURES)];
    for(size_t i = 0; i < test_count; i++) {
        tests[i] = (struct CMUnitTest) {
            .name = DERIVE_ADDRESS_REJECT_FIXTURES[i].name,
            .test_func = test_derive_address_reject_fixture,
            .initial_state = (void *) &DERIVE_ADDRESS_REJECT_FIXTURES[i],
        };
    }
    cmocka_run_group_tests(tests, NULL, NULL);
}