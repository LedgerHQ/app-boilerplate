#include <string.h>  // explicit_bzero

#include "io.h"
#include "globals.h"
#include "utils/cardano_os_utils.h"

int send_error_and_reset(uint16_t sw) {
    G_context.req_type = REQUEST_NONE;
    explicit_bzero(&G_context.state, sizeof(G_context.state));
    return io_send_sw(sw);
}
