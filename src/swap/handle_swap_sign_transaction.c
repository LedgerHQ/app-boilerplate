#ifdef HAVE_SWAP
#include "swap.h"
#include "swap_error_code_helpers.h"
#include "os.h"
#include "format.h"

#include "sw.h"
#include "globals.h"
#include "tx_types.h"
#include "handle_swap.h"
#include "tokens.h"

#include <stdint.h>

// This is a smart documentation inclusion. The full documentation is available at
// https://ledgerhq.github.io/app-exchange/
// --8<-- [start:swap_copy_transaction_parameters]
typedef struct swap_validated_s {
    bool initialized;
    uint64_t amount;
    uint64_t fee;
    char recipient[ADDRESS_LEN * 2 + 1];
    uint8_t decimals;
    char ticker[MAX_TICKER_SIZE];
} swap_validated_t;

/* Global variable used to store swap validation status */
static swap_validated_t G_swap_validated;

static bool is_token_swap(void) {
    return (strcmp(G_swap_validated.ticker, "BOL") != 0 ||
            G_swap_validated.decimals != EXPONENT_SMALLEST_UNIT);
}

bool swap_copy_transaction_parameters(create_transaction_parameters_t* params) {
    PRINTF("Inside swap_copy_transaction_parameters\n");

    if (params->destination_address == NULL) {
        PRINTF("Destination address expected\n");
        return false;
    }

    if (strlen(params->destination_address) != (ADDRESS_LEN * 2)) {
        PRINTF("Destination address wrong length\n");
        return false;
    }

    if (params->amount == NULL) {
        PRINTF("Amount expected\n");
        return false;
    }

    // first copy parameters to stack, and then to global data.
    // We need this "trick" as the input data position can overlap with app globals
    // and also because we want to memset the whole bss segment as it is not done
    // when an app is called as a lib.
    // This is necessary as many part of the code expect bss variables to
    // initialized at 0.
    swap_validated_t swap_validated;
    explicit_bzero(&swap_validated, sizeof(swap_validated));

    // Save recipient as an uppercase string
    for (int i = 0; i < ADDRESS_LEN * 2; i++) {
        if (params->destination_address[i] >= 'a' && params->destination_address[i] <= 'z') {
            swap_validated.recipient[i] = params->destination_address[i] - 'a' + 'A';
        } else {
            swap_validated.recipient[i] = params->destination_address[i];
        }
    }
    PRINTF("Validated recipient: %s\n", swap_validated.recipient);

    // Parse config and save decimals and ticker
    // If there is no coin_configuration, consider that we are doing a SOL swap
    if (params->coin_configuration == NULL) {
        memcpy(swap_validated.ticker, "BOL", sizeof("BOL"));
        swap_validated.decimals = EXPONENT_SMALLEST_UNIT;
    } else {
        if (!swap_parse_config(params->coin_configuration,
                               params->coin_configuration_length,
                               swap_validated.ticker,
                               sizeof(swap_validated.ticker),
                               &swap_validated.decimals)) {
            PRINTF("Fail to parse coin_configuration\n");
            return false;
        }
    }
    PRINTF("Validated ticker: %s, decimals: %d\n", swap_validated.ticker, swap_validated.decimals);

    // Save amount
    if (!swap_str_to_u64(params->amount, params->amount_length, &swap_validated.amount)) {
        PRINTF("Failed to convert amount to uint64_t\n");
        return false;
    }
    // Can't print u64
    PRINTF("Validated amount: %.*H\n", sizeof(swap_validated.amount), &swap_validated.amount);

    // Save fee
    if (!swap_str_to_u64(params->fee_amount, params->fee_amount_length, &swap_validated.fee)) {
        PRINTF("Failed to convert fee to uint64_t\n");
        return false;
    }
    PRINTF("Validated fee: %.*H\n", sizeof(swap_validated.fee), &swap_validated.fee);

    swap_validated.initialized = true;

    // Full reset the global variables
    os_explicit_zero_BSS_segment();

    // Commit from stack to global data, params becomes tainted but we won't access it anymore
    memcpy(&G_swap_validated, &swap_validated, sizeof(swap_validated));

    return true;
}
// --8<-- [end:swap_copy_transaction_parameters]

// This is a smart documentation inclusion. The full documentation is available at
// https://ledgerhq.github.io/app-exchange/
// --8<-- [start:swap_check_validity]
/* Check if the Tx to sign have the same parameters as the ones previously validated */
bool swap_check_validity(uint64_t amount,
                         uint64_t fee,
                         const uint8_t* destination,
                         const token_info_t* token_info) {
    PRINTF("Inside swap_check_validity\n");

    if (!G_swap_validated.initialized) {
        PRINTF("Swap structure is not initialized\n");
        send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_GENERIC, SWAP_ERROR_CODE);
        // unreachable
        os_sched_exit(0);
    }

    // Reject token transactions in swap context
    if (G_context.tx_info.is_token_tx) {
        if (is_token_swap()) {
            // Check that the token is the expected one
            if (strcmp(G_swap_validated.ticker, token_info->ticker) != 0 ||
                G_swap_validated.decimals != token_info->decimals) {
                PRINTF("Token info does not match\n");
                PRINTF("Validated: %s (decimals: %d)\n",
                       G_swap_validated.ticker,
                       G_swap_validated.decimals);
                PRINTF("Received: %s (decimals: %d)\n", token_info->ticker, token_info->decimals);
                send_swap_error_simple(SW_SWAP_FAIL,
                                       SWAP_EC_ERROR_WRONG_AMOUNT,
                                       SWAP_ERROR_WRONG_TOKEN_INFO);
                // unreachable
                os_sched_exit(0);
            } else {
                PRINTF("Token info match\n");
            }
        } else {
            PRINTF("Unexpected token transaction from swap context\n");
            send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_WRONG_METHOD, SWAP_ERROR_CODE);
            // unreachable
            os_sched_exit(0);
        }
    } else if (is_token_swap()) {
        PRINTF("Token transactions expected from swap context\n");
        send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_WRONG_METHOD, SWAP_ERROR_CODE);
        // unreachable
        os_sched_exit(0);
    }

    if (G_swap_validated.amount != amount) {
        PRINTF("Amount does not match, promised %lld, received %lld\n",
               G_swap_validated.amount,
               amount);
        send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_WRONG_AMOUNT, SWAP_ERROR_CODE);
        // unreachable
        os_sched_exit(0);
    } else {
        PRINTF("Amounts match \n");
    }

    if (G_swap_validated.fee != fee) {
        PRINTF("Fee does not match, promised %lld, received %lld\n", G_swap_validated.fee, fee);
        send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_WRONG_FEES, SWAP_ERROR_CODE);
        // unreachable
        os_sched_exit(0);
    } else {
        PRINTF("Fees match \n");
    }

    char to[ADDRESS_LEN * 2 + 1] = {0};
    format_hex(destination, ADDRESS_LEN, to, sizeof(to));
    if (strcmp(G_swap_validated.recipient, to) != 0) {
        PRINTF("Destination does not match\n");
        PRINTF("Validated: %s\n", G_swap_validated.recipient);
        PRINTF("Received: %s \n", to);
        send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_WRONG_DESTINATION, SWAP_ERROR_CODE);
        // unreachable
        os_sched_exit(0);
    } else {
        PRINTF("Destination is valid\n");
    }
    return true;
}
// --8<-- [end:swap_check_validity]

#endif  // HAVE_SWAP
