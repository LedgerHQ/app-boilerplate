#include <string.h>  // explicit_bzero

#include "io.h"
#include "globals.h"
#include "utils/cardano_os_utils.h"
#include "utils/utils.h"

int send_error_and_reset(uint16_t sw) {
    TRACE("send_error_and_reset sw=0x%04x", sw);
    G_context.req_type = REQUEST_NONE;
    explicit_bzero(&G_context.state, sizeof(G_context.state));
    io_send_sw(sw);
    // Return 0 to indicate that the command was processed (responded with error)
    // and the main loop should continue.
    return 0;
}
