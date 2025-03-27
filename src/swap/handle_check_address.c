#ifdef HAVE_SWAP
#include "swap.h"
#include "buffer.h"
#include "bip32.h"
#include "crypto_helpers.h"
#include "cx.h"
#include "os.h"

#include "types.h"
#include "format.h"
#include "address.h"
#include "tx_types.h"

#include <string.h>

/* Check that the address used to receive funds is owned by the device
 * check_address_parameters_t is defined in C SDK as:
 * struct {
 *   // IN
 *   uint8_t *coin_configuration;
 *   uint8_t  coin_configuration_length;
 *   // serialized path, segwit, version prefix, hash used, dictionary etc.
 *   // fields and serialization format depends on specific coin app
 *   uint8_t *address_parameters;
 *   uint8_t  address_parameters_length;
 *   char    *address_to_check;
 *   char    *extra_id_to_check;
 *   // OUT
 *   int result;
 * } check_address_parameters_t;
 */
void swap_handle_check_address(check_address_parameters_t *params) {
    PRINTF("Inside swap_handle_check_address\n");
    params->result = 0;

    if (params->address_parameters == NULL) {
        PRINTF("derivation path expected\n");
        return;
    }
    PRINTF("address_parameters %.*H\n",
           params->address_parameters_length,
           params->address_parameters);

    if (params->address_to_check == NULL) {
        PRINTF("Address to check expected\n");
        return;
    }
    PRINTF("Address to check %s\n", params->address_to_check);
    if (strlen(params->address_to_check) != (ADDRESS_LEN * 2)) {
        PRINTF("Address to check expected length %d, not %d\n",
               ADDRESS_LEN * 2,
               strlen(params->address_to_check));
        return;
    }

    buffer_t buf = {.ptr = params->address_parameters,
                    .size = params->address_parameters_length,
                    .offset = 0};

    uint8_t bip32_path_len;
    uint32_t bip32_path[MAX_BIP32_PATH];
    pubkey_ctx_t pk_info = {0};

    buffer_read_u8(&buf, &bip32_path_len);
    buffer_read_bip32_path(&buf, bip32_path, (size_t) bip32_path_len);

    cx_err_t ret = bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                               bip32_path,
                                               bip32_path_len,
                                               pk_info.raw_public_key,
                                               pk_info.chain_code,
                                               CX_SHA512);
    if (ret != CX_OK) {
        PRINTF("Failed to derive public key\n");
        return;
    }

    uint8_t address[ADDRESS_LEN] = {0};
    address_from_pubkey(pk_info.raw_public_key, address, sizeof(address));

    char derived_address[41];
    memset(derived_address, 0, sizeof(derived_address));
    format_hex(address, sizeof(address), derived_address, sizeof(derived_address));
    PRINTF("Derived address %s\n", derived_address);

    PRINTF("Checked address %s\n", params->address_to_check);

    if (strncmp(derived_address, params->address_to_check, sizeof(derived_address)) != 0) {
        PRINTF("Addresses do not match\n");
    } else {
        PRINTF("Addresses match\n");
        params->result = 1;
    }
}
#endif  // HAVE_SWAP
