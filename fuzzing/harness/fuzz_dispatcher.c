/*
 * Boilerplate fuzz harness: one APDU per iteration via the framework's
 * fuzz_harness_entry(), plus a swap-callback lane gated on fuzz_ctrl[2].
 *
 * Absolution drives global state through the invariant domains; this file
 * only wires the app's command table and app-specific callbacks.
 */

#include "mocks.h"
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
const internal_storage_t N_storage_real;

#ifdef HAVE_SWAP
static volatile uint8_t _swap_return_dummy;
#endif

/* ── Layout + framework mutator ───────────────────────────────────────────── */

#include "scenario_layout.h"

#define FUZZ_PREFIX_SIZE_FALLBACK SCEN_PREFIX_SIZE
#define FUZZ_CTRL_OFF             SCEN_CTRL_OFF
#define FUZZ_CTRL_LEN             SCEN_CTRL_LEN
#define fuzz_lane_is_structured(data, ps) \
    ((ps) > FUZZ_CTRL_OFF && (data)[FUZZ_CTRL_OFF] > FUZZ_STRUCTURED_LANE_THRESHOLD)

#include "fuzz_mutator.h"
#include "fuzz_layout_check.h"

size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t max_size, unsigned int seed) {
    return fuzz_custom_mutator(data, size, max_size, seed);
}

/* ── App adapter ──────────────────────────────────────────────────────────── */

#include "fuzz_harness.h"

const fuzz_command_spec_t fuzz_commands[] = {
    {.cla = CLA, .ins = GET_VERSION},
    {.cla = CLA, .ins = GET_APP_NAME},
    {.cla = CLA, .ins = GET_PUBLIC_KEY, .p1_max = 1, .flags = FUZZ_CMD_HAS_DATA},
    {.cla = CLA, .ins = SIGN_TX, .p1_max = 3, .p2_max = 1, .flags = FUZZ_CMD_HAS_DATA},
    {.cla = CLA, .ins = SIGN_TOKEN_TX, .p1_max = 3, .p2_max = 1, .flags = FUZZ_CMD_HAS_DATA},
    {.cla = CLA, .ins = PROVIDE_TOKEN_INFO, .flags = FUZZ_CMD_HAS_DATA},
};

const size_t fuzz_n_commands = sizeof(fuzz_commands) / sizeof(fuzz_commands[0]);

void fuzz_app_reset(void) {
}

void fuzz_app_dispatch(void *cmd) {
    command_t *c = (command_t *) cmd;
    if (c->ins == SIGN_TX || c->ins == SIGN_TOKEN_TX) {
        c->p2 = c->p2 ? 0x80 : 0x00;
    }
    apdu_dispatcher((const command_t *) c);
}

/* ── Swap library callback mode ───────────────────────────────────────────── */

#ifdef HAVE_SWAP
#define SWAP_MODE_THRESHOLD 192

static void fuzz_swap_callbacks(const uint8_t *tail, size_t tail_len) {
    if (tail_len < 2) return;

    uint8_t sub_mode = tail[0] % 3;
    const uint8_t *payload = tail + 1;
    size_t plen = tail_len - 1;

    char addr_str[ADDRESS_LEN * 2 + 1];
    memset(addr_str, 0, sizeof(addr_str));
    size_t copy_len = plen < (ADDRESS_LEN * 2) ? plen : (ADDRESS_LEN * 2);
    memcpy(addr_str, payload, copy_len);

    switch (sub_mode) {
        case 0: {
            check_address_parameters_t params;
            memset(&params, 0, sizeof(params));
            params.address_parameters = (uint8_t *) payload;
            params.address_parameters_length = (uint8_t) (plen > 255 ? 255 : plen);
            params.address_to_check = addr_str;
            params.extra_id_to_check = (char *) "";
            swap_handle_check_address(&params);
            break;
        }
        case 1: {
            get_printable_amount_parameters_t params;
            memset(&params, 0, sizeof(params));
            params.amount = (uint8_t *) payload;
            params.amount_length = (uint8_t) (plen > 16 ? 16 : plen);
            params.is_fee = (plen > 16) ? payload[16] & 1 : false;
            swap_handle_get_printable_amount(&params);
            break;
        }
        case 2: {
            create_transaction_parameters_t params;
            memset(&params, 0, sizeof(params));
            params.destination_address = addr_str;
            params.amount = (uint8_t *) payload;
            params.amount_length = (uint8_t) (plen > 16 ? 16 : plen);
            params.fee_amount = (plen > 16) ? (uint8_t *) (payload + 16) : (uint8_t *) payload;
            params.fee_amount_length = (plen > 32) ? 16 : (uint8_t) (plen > 16 ? plen - 16 : plen);
            swap_copy_transaction_parameters(&params);
            break;
        }
    }
}
#endif /* HAVE_SWAP */

/* ── Fuzz entry point ─────────────────────────────────────────────────────── */

int fuzz_entry(const uint8_t *data, size_t size) {
#ifdef HAVE_SWAP
    G_swap_signing_return_value_address = &_swap_return_dummy;

    if (size >= 4 && fuzz_ctrl[2] >= SWAP_MODE_THRESHOLD) {
        if (sigsetjmp(fuzz_exit_jump_ctx.jmp_buf, 1)) return 0;
        fuzz_swap_callbacks(data, size);
        return 0;
    }
#endif

    return fuzz_harness_entry(data, size);
}
