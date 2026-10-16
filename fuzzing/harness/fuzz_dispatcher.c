/* Boilerplate fuzz harness: the app's command table plus a swap-callback lane.
 * App state comes from the invariant, so there is no per-iteration setup. */

#include "globals.h"
#include "dispatcher.h"
#include "constants.h"
#include "types.h"

#include <stdint.h>
#include <string.h>

#ifdef HAVE_SWAP
#include "swap.h"
#include "handle_swap.h"
#include "swap_utils.h"
#endif

global_ctx_t G_context;

/* globals.h declares this extern const, so the definition stays const. Giving it
 * the post-install values keeps it out of .rodata trouble: app_main() is excluded
 * here, so nothing runs its nvm_write() first-boot branch. */
const internal_storage_t N_storage_real = {
    .initialized = 0x01,
    .dummy1_allowed = 0x00,
    .dummy2_allowed = 0x00,
};

#ifdef HAVE_SWAP
static volatile uint8_t _swap_return_dummy;

/* Own entry point, to add the swap lane on top of the APDU path. */
#define FUZZ_APP_CUSTOM_ENTRY
#endif

#include "fuzz_harness.h"

const fuzz_command_spec_t fuzz_commands[] = {
    {.cla = CLA, .ins = GET_VERSION},
    {.cla = CLA, .ins = GET_APP_NAME},
    {.cla = CLA, .ins = GET_PUBLIC_KEY, .p1_max = 1, .flags = FUZZ_CMD_HAS_DATA},
    {.cla = CLA, .ins = SIGN_TX, .p1_max = P1_MAX, .p2_max = 1, .flags = FUZZ_CMD_HAS_DATA},
    {.cla = CLA, .ins = SIGN_TOKEN_TX, .p1_max = P1_MAX, .p2_max = 1, .flags = FUZZ_CMD_HAS_DATA},
    {.cla = CLA, .ins = PROVIDE_TOKEN_INFO, .flags = FUZZ_CMD_HAS_DATA},
};

FUZZ_COMMAND_COUNT();

void fuzz_app_dispatch(void *cmd) {
    command_t *c = (command_t *) cmd;

    /* P2 carries a flag, not a range: fuzz_clamp_p() reduces the raw byte modulo
     * p2_max + 1, so map its two values onto the two the dispatcher accepts. */
    if (c->ins == SIGN_TX || c->ins == SIGN_TOKEN_TX) {
        if (c->p2 == 0) {
            c->p2 = P2_LAST;
        } else {
            c->p2 = P2_MORE;
        }
    }
    apdu_dispatcher(c);
}

#ifdef HAVE_SWAP
/* The swap library callbacks are entered from the Exchange app, never through an
 * APDU, so a lane keyed on the first app byte reaches them. */
#define SWAP_MODE_THRESHOLD 192

static uint8_t clamp_u8(size_t v, uint8_t max) {
    if (v > max) {
        return max;
    }
    return (uint8_t) v;
}

static void fuzz_swap_callbacks(const uint8_t *tail, size_t tail_len) {
    if (tail_len < 2) {
        return;
    }

    const uint8_t *payload = tail + 1;
    size_t plen = tail_len - 1;

    char addr[ADDRESS_LEN * 2 + 1];
    memset(addr, 0, sizeof(addr));
    memcpy(addr, payload, clamp_u8(plen, sizeof(addr) - 1));

    switch (tail[0] % 3) {
        case 0: {
            check_address_parameters_t p = {.address_parameters = (uint8_t *) payload,
                                           .address_parameters_length = clamp_u8(plen, UINT8_MAX),
                                           .address_to_check = addr,
                                           .extra_id_to_check = (char *) ""};
            swap_handle_check_address(&p);
            break;
        }
        case 1: {
            get_printable_amount_parameters_t p = {
                .amount = (uint8_t *) payload,
                .amount_length = clamp_u8(plen, 16),
                .is_fee = (plen > 16) && ((payload[16] & 1) != 0)};
            swap_handle_get_printable_amount(&p);
            break;
        }
        default: {
            create_transaction_parameters_t p = {.destination_address = addr,
                                                 .amount = (uint8_t *) payload,
                                                 .amount_length = clamp_u8(plen, 16),
                                                 .fee_amount = (uint8_t *) payload,
                                                 .fee_amount_length = clamp_u8(plen, 16)};
            swap_copy_transaction_parameters(&p);
            break;
        }
    }
}

int fuzz_entry(const uint8_t *data, size_t size) {
    G_swap_signing_return_value_address = &_swap_return_dummy;

    if (size > FUZZ_CTRL_LEN && data[FUZZ_CTRL_LEN] >= SWAP_MODE_THRESHOLD) {
        if (sigsetjmp(fuzz_exit_jump_ctx.jmp_buf, 1)) {
            return 0;
        }
        fuzz_swap_callbacks(data + FUZZ_CTRL_LEN, size - FUZZ_CTRL_LEN);
        return 0;
    }

    return fuzz_harness_entry(data, size);
}
#endif /* HAVE_SWAP */
