#include "io.h"
#include "sw.h"
#include "noop.h"

int handler_noop(void) {
    return io_send_sw(SWO_SUCCESS);
}
