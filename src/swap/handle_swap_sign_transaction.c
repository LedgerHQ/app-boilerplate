#ifdef HAVE_SWAP
#include "swap.h"
#include "swap_error_code_helpers.h"
#include "os.h"
#include "format.h"

#include "sw.h"
#include "globals.h"
#include "tx_types.h"
#include "handle_swap.h"

#include <stdint.h>

/* Global variable used to store swap validation status */
swap_validated_t G_swap_validated;

bool swap_copy_transaction_parameters(create_transaction_parameters_t* params) {
    PRINTF("Inside swap_copy_transaction_parameters %s\n", params->destination_address);

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
    memset(&swap_validated, 0, sizeof(swap_validated));

    // Save recipient as an uppercase string
    for (int i = 0; i < ADDRESS_LEN * 2; i++) {
        if (params->destination_address[i] >= 'a' && params->destination_address[i] <= 'z') {
            swap_validated.recipient[i] = params->destination_address[i] - 'a' + 'A';
        } else {
            swap_validated.recipient[i] = params->destination_address[i];
        }
    }

    // Save amount
    if (!swap_str_to_u64(params->amount, params->amount_length, &swap_validated.amount)) {
        PRINTF("Failed to convert amount to uint64_t\n");
        return false;
    }

    // Save fee
    if (!swap_str_to_u64(params->fee_amount, params->fee_amount_length, &swap_validated.fee)) {
        PRINTF("Failed to convert fee to uint64_t\n");
        return false;
    }

    swap_validated.initialized = true;

    // Full reset the global variables
    os_explicit_zero_BSS_segment();

    // Commit from stack to global data, params becomes tainted but we won't access it anymore
    memcpy(&G_swap_validated, &swap_validated, sizeof(swap_validated));

    PRINTF("Out of swap_copy_transaction_parameters\n");

    return true;
}

/* Check if the Tx to sign have the same parameters as the ones previously validated */
bool swap_check_validity(bool initialized,
                         uint64_t amount,
                         uint64_t fee,
                         const char *recipient) {
    PRINTF("Inside swap_check_validity\n");

    if (!initialized) {
        PRINTF("Swap structure is not initialized\n");
        send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_GENERIC, SWAP_ERROR_CODE);
        // unreachable
        os_sched_exit(0);
    }

    if (amount != G_context.tx_info.transaction.value) {
        PRINTF("Amount does not match, promised %lld, received %lld\n",
               amount,
               G_context.tx_info.transaction.value);
        send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_WRONG_AMOUNT, SWAP_ERROR_CODE);
        // unreachable
        os_sched_exit(0);
    } else {
        PRINTF("Amounts match \n");
    }

    char to[ADDRESS_LEN * 2 + 1] = {0};
    format_hex(G_context.tx_info.transaction.to, ADDRESS_LEN, to, sizeof(to));
    if (strcmp(recipient, to) != 0) {
        PRINTF("Destination does not match\n");
        PRINTF("Validated: %s\n", recipient);
        PRINTF("Received: %s \n", to);
        send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_WRONG_DESTINATION, SWAP_ERROR_CODE);
        // unreachable
        os_sched_exit(0);
    } else {
        PRINTF("Destination is valid\n");
    }
    return true;
}
#endif  // HAVE_SWAP