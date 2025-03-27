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

typedef struct swap_validated_s {
    bool initialized;
    uint64_t amount;
    uint64_t fee;
    char recipient[ADDRESS_LEN * 2 + 1];
} swap_validated_t;

static swap_validated_t G_swap_validated;

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
    if (params->amount_length > sizeof(swap_validated.amount)) {
        PRINTF("Amount too big\n");
        return false;
    } else {
        // Convert params->amount to uint64_t
        swap_validated.amount = 0;
        memcpy(((uint8_t*) &swap_validated.amount) + sizeof(swap_validated.amount) -
                   params->amount_length,
               params->amount,
               params->amount_length);
        swap_validated.amount = __builtin_bswap64(swap_validated.amount);
    }

    // Save fee
    if (params->fee_amount_length > sizeof(swap_validated.fee)) {
        PRINTF("Fee too big\n");
        return false;
    } else {
        // Convert params->fee_amount to uint64_t
        swap_validated.fee = 0;
        memcpy(((uint8_t*) &swap_validated.fee) + sizeof(swap_validated.fee) -
                   params->fee_amount_length,
               params->fee_amount,
               params->fee_amount_length);
        swap_validated.fee = __builtin_bswap64(swap_validated.fee);
    }

    swap_validated.initialized = true;

    // Full reset the global variables
    os_explicit_zero_BSS_segment();

    // Commit from stack to global data, params becomes tainted but we won't access it anymore
    memcpy(&G_swap_validated, &swap_validated, sizeof(swap_validated));

    PRINTF("Out of swap_copy_transaction_parameters\n");

    return true;
}

bool swap_check_validity() {
    PRINTF("Inside swap_check_validity\n");

    if (!G_swap_validated.initialized) {
        PRINTF("Swap structure is not initialized\n");
        send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_GENERIC, SWAP_ERROR_CODE);
        // unreachable
        os_sched_exit(0);
    }

    if (G_swap_validated.amount != G_context.tx_info.transaction.value) {
        PRINTF("Amount does not match, promised %lld, received %lld\n",
               G_swap_validated.amount,
               G_context.tx_info.transaction.value);
        send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_WRONG_AMOUNT, SWAP_ERROR_CODE);
        // unreachable
        os_sched_exit(0);
    } else {
        PRINTF("Amounts match \n");
    }

    char to[ADDRESS_LEN * 2 + 1] = {0};
    format_hex(G_context.tx_info.transaction.to, ADDRESS_LEN, to, sizeof(to));
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
#endif  // HAVE_SWAP