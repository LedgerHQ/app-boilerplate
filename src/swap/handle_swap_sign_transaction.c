#ifdef HAVE_SWAP
#include "swap.h"
#include "os.h"

#include "handle_swap.h"

#include <stdint.h>

typedef struct swap_validated_s {
    bool initialized;
    uint8_t amount_length;
    uint8_t amount[30];
    uint8_t fee_length;
    uint8_t fee[30];
    char recipient[ADDRESS_LENGTH + 1];
} swap_validated_t;

static swap_validated_t G_swap_validated;

// Save the BSS address where we will write the return value when finished
static uint8_t* G_swap_sign_return_value_address;

bool swap_copy_transaction_parameters(create_transaction_parameters_t* params){
    PRINTF("Inside swap_copy_transaction_parameters\n");

    // Ensure no extraid
    // if (params->destination_address_extra_id == NULL) {
    //     PRINTF("destination_address_extra_id expected\n");
    //     return false;
    // } else if (params->destination_address_extra_id[0] != '\0') {
    //     PRINTF("destination_address_extra_id expected empty, not '%s'\n",
    //            params->destination_address_extra_id);
    //     return false;
    // }

    if (params->destination_address == NULL) {
        PRINTF("Destination address expected\n");
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

    // Parse config and save decimals and ticker
    // If there is no coin_configuration, consider that we are doing a TRX swap
    // if (params->coin_configuration == NULL) {
    //     memcpy(swap_validated.ticker, "TON", sizeof("TON"));
    //     swap_validated.decimals = EXPONENT_SMALLEST_UNIT;
    // } else {
    //     if (!swap_parse_config(params->coin_configuration,
    //                            params->coin_configuration_length,
    //                            swap_validated.ticker,
    //                            sizeof(swap_validated.ticker),
    //                            &swap_validated.decimals)) {
    //         PRINTF("Fail to parse coin_configuration\n");
    //         return false;
    //     }
    // }

    // Save recipient
    strlcpy(swap_validated.recipient,
            params->destination_address,
            sizeof(swap_validated.recipient));
    if (swap_validated.recipient[sizeof(swap_validated.recipient) - 1] != '\0') {
        PRINTF("Address copy error\n");
        return false;
    }

    // Save amount
    if (params->amount_length > sizeof(swap_validated.amount)) {
        PRINTF("Amount too big\n");
        return false;
    } else {
        swap_validated.amount_length = params->amount_length;
        memcpy(swap_validated.amount, params->amount, params->amount_length);
    }

    swap_validated.initialized = true;

    // Full reset the global variables
    os_explicit_zero_BSS_segment();

    // Keep the address at which we'll reply the signing status
    G_swap_sign_return_value_address = &params->result;

    // Commit from stack to global data, params becomes tainted but we won't access it anymore
    memcpy(&G_swap_validated, &swap_validated, sizeof(swap_validated));
    return true;
}
#endif // HAVE_SWAP
