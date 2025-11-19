#ifdef HAVE_SWAP
#include "swap.h"
#include "buffer.h"
#include "constants.h"
#include "format.h"
#include "os.h"
#include "tokens.h"

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

    explicit_bzero(params->printable_amount, sizeof(params->printable_amount));

    /// Convert params->amount into uint64_t
    uint64_t raw_amount = 0;
    if (!swap_str_to_u64(params->amount, params->amount_length, &raw_amount)) {
        PRINTF("Amount is too big\n");
        return;
    }

    uint8_t decimals = EXPONENT_SMALLEST_UNIT;
    char ticker[MAX_TICKER_SIZE] = "BOL";
    if (params->is_fee || params->coin_configuration == NULL) {
        PRINTF("Defaulting to native BOL amount\n");
    } else {
        if (!swap_parse_config(params->coin_configuration,
                               params->coin_configuration_length,
                               ticker,
                               sizeof(ticker),
                               &decimals)) {
            PRINTF("Fail to parse coin_configuration\n");
            return;
        }
    }
    char formatted_amount[30] = {0};
    format_fpu64(formatted_amount, sizeof(formatted_amount), raw_amount, decimals);
    PRINTF("Formatted amount: %s\n", formatted_amount);
    snprintf(params->printable_amount,
             sizeof(params->printable_amount),
             "%.*s %s",
             (int) strlen(formatted_amount),
             formatted_amount,
             ticker);
}
#endif  // HAVE_SWAP
