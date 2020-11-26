#include <stdint.h>
#include <string.h>
#include <sys/types.h>

extern "C" {
#include "common/buffer.h"
#include "transaction/deserialize.h"
#include "transaction/types.h"
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    buffer_t buf = {.ptr = data, .size = size, .offset = 0};
    transaction_t tx;

    memset(&tx, 0, sizeof(tx));
    transaction_deserialize(&buf, &tx);

    return 0;
}
