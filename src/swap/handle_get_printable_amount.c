#ifdef HAVE_SWAP
#include "swap.h"
#include "buffer.h"
#include "constants.h"
#include "format.h"
#include "os.h"

#include <string.h>
#include <stdio.h>

/* Format printable amount including the ticker from specified parameters.
 *
 * Must set empty printable_amount on error, printable amount otherwise
 * get_printable_amount_parameters_t is defined in C SDK as:
 * struct {
 *   // IN
 *   uint8_t *coin_configuration;
 *   uint8_t  coin_configuration_length;
 *   uint8_t *amount;
 *   uint8_t  amount_length;
 *   bool     is_fee;
 *   // OUT
 *   char printable_amount[MAX_PRINTABLE_AMOUNT_SIZE];
 * } get_printable_amount_parameters_t;
 */
void swap_handle_get_printable_amount(get_printable_amount_parameters_t* params) {
    PRINTF("Inside swap_handle_get_printable_amount\n");

    PRINTF("Amount: %.*H\n", params->amount_length, params->amount);

    char amount[30] = {0};
    memset(params->printable_amount, 0, sizeof(params->printable_amount));

    /// Convert params->amount into uint64_t
    uint64_t value = 0;
    if (swap_str_to_u64(params->amount, params->amount_length, &value)) {
       format_fpu64(amount, sizeof(amount), value, EXPONENT_SMALLEST_UNIT);
       PRINTF("Formatted amount: %s\n", amount);
       snprintf(params->printable_amount,
                sizeof(params->printable_amount),
                "BOL %.*s",
                sizeof(amount),
                amount);
    }
    else {
        PRINTF("Failed to convert amount to uint64_t\n");
    }
}
#endif  // HAVE_SWAP