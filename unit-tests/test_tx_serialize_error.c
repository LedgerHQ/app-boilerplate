/*
 * Unit tests for src/transaction/serialize.c error paths that the real SDK
 * helpers cannot reach: varint_write() and write_u64_be() are mocked.
 * The other transaction tests run against the real helpers in test_tx_parser.c.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mockwrite.h"
#include "Mockvarint.h"

#include "serialize.h"
#include "types.h"

void setUp(void) {
    Mockwrite_Init();
    Mockvarint_Init();
}

void tearDown(void) {
    Mockwrite_Verify();
    Mockvarint_Verify();
    Mockwrite_Destroy();
    Mockvarint_Destroy();
}

void test_serialize_varint_write_error(void) {
    uint8_t to[ADDRESS_LEN] = {0};
    uint8_t memo[] = {'a', 'b', 'c'};
    transaction_t tx = {0};
    tx.nonce = 1;
    tx.value = 2;
    tx.to = to;
    tx.memo = memo;
    tx.memo_len = sizeof(memo);
    uint8_t out[MAX_TX_LEN];

    varint_size_ExpectAndReturn(sizeof(memo), 1);
    write_u64_be_Ignore();
    varint_write_ExpectAnyArgsAndReturn(-1);  // varint_write fails

    TEST_ASSERT_EQUAL_INT(-1, transaction_serialize(&tx, out, sizeof(out)));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_serialize_varint_write_error);

    return UNITY_END();
}
