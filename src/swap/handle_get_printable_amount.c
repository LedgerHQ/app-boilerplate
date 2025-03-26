#ifdef HAVE_SWAP
#include "swap.h"
#include "buffer.h"
#include "constants.h"
#include "format.h"
#include "os.h"

#include <string.h>
#include <stdio.h>

void swap_handle_get_printable_amount(get_printable_amount_parameters_t* params) {
    PRINTF("Inside swap_handle_get_printable_amount\n");

    PRINTF("Amount: %.*H\n", params->amount_length, params->amount);

    uint64_t value = 0;
    char amount[30] = {0};

    buffer_t buf = { .ptr = params->amount, .size = params->amount_length, .offset = 0};

    /* convert the buffer params->amount into a integer */
    uint8_t byte;
    while (buf.offset < buf.size) {
        buffer_read_u8(&buf, &byte);
        value = (value << 8) | byte;
    }
    
    format_fpu64(amount, sizeof(amount), value, EXPONENT_SMALLEST_UNIT);

    PRINTF("Formatted amount: %s\n", amount);

    memset(params->printable_amount, 0, sizeof(params->printable_amount));
    snprintf(params->printable_amount,
        sizeof(params->printable_amount),
        "BOL %.*s",
        sizeof(amount), amount);
}
#endif // HAVE_SWAP