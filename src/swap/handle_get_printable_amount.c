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

    char amount[30] = {0};

    /// Convert params->amount into uint64_t
    uint64_t value = 0;
    memcpy(((uint8_t*) &value) + sizeof(value) - params->amount_length,
               params->amount,
               params->amount_length);
    value = __builtin_bswap64(value);

    format_fpu64(amount, sizeof(amount), value, EXPONENT_SMALLEST_UNIT);

    PRINTF("Formatted amount: %s\n", amount);

    memset(params->printable_amount, 0, sizeof(params->printable_amount));
    snprintf(params->printable_amount,
             sizeof(params->printable_amount),
             "BOL %.*s",
             sizeof(amount),
             amount);
}
#endif  // HAVE_SWAP