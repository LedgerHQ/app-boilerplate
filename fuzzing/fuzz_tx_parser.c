#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "transaction/deserialize.h"
#include "transaction/utils.h"
#include "transaction/tx_types.h"
#include "format.h"
#include "mock/mocks.h"
#include <setjmp.h>

jmp_buf fuzz_exit_jump_buf;
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (setjmp(fuzz_exit_jump_ctx.jmp_buf) == 0) {
        buffer_t buf = {.ptr = data, .size = size, .offset = 0};
        transaction_t tx;
        parser_status_e status;
        char nonce[21] = {0};
        char address[21] = {0};
        char amount[21] = {0};
        char tx_memo[466] = {0};

        memset(&tx, 0, sizeof(tx));

        status = transaction_deserialize(&buf, &tx);

        if (status == PARSING_OK) {
            format_u64(nonce, sizeof(nonce), tx.nonce);
            // printf("nonce: %s\n", nonce);
            format_hex(tx.to, ADDRESS_LEN, address, sizeof(address));
            // printf("address: %s\n", address);
            format_fpu64(amount, sizeof(amount), tx.value, 3);  // exponent of smallest unit is 3
            // printf("amount: %s\n", amount);
            transaction_utils_format_memo(tx.memo, tx.memo_len, tx_memo, sizeof(tx_memo));
            // printf("memo: %s\n", tx_memo);
        }
    }
    return 0;
}
