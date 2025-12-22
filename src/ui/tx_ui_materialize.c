#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "os.h"
#include "glyphs.h"
#include "nbgl_use_case.h"
#include "cardano_swo.h"
#include "globals.h"
#include "ui/ui_constants.h"
#include "tx_output_types.h"
#include "transaction/tx.h"
#include "addressUtils/addressUtilsShelley.h"
#include "memory/mem.h"
#include "securityPolicy/securityPolicy.h"
#include "securityPolicy/securityWarnings.h"
#include "utils/assert.h"
#include "utils/textUtils.h"
#include "ui/display.h"
#include "ui/ui_utils.h"
#include "io.h"
#include "utils/cardano_os_utils.h"
#include "app_tokens/app_tokens.h"

static nbgl_warning_t *g_warning = NULL;

static char *ui_alloc_temp(size_t size) {
    if (size == 0) {
        return NULL;
    }
    char *tmp = (char *) app_mem_alloc(size);
    if (tmp != NULL) {
        explicit_bzero(tmp, size);
    }
    return tmp;
}

static int ui_add_pair_or_fail(const char *label, char *tmp_buf) {
    if (!ui_pairs_add(label, tmp_buf)) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    return SWO_SUCCESS;
}

static int ui_materialize_strings(void) {
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_HASHED, "String materialization invoked too early");
    transaction_t *tx = &G_context.tx_info.transaction;
    int status;

    uint16_t output_num = 1;
    s_flist_node *output_node = tx->outputs;
    while (output_node != NULL) {
        tx_output_list_item_t *output_item = (tx_output_list_item_t *) output_node;
        s_flist_node *next = output_node->next;

        tx_output_description_t output_desc = {
            .format = output_item->output_data.format,
            .amount = output_item->output_data.adaAmount,
            .numAssetGroups = output_item->output_data.numAssetGroups,
            .includeDatum = (output_item->output_data.datum.type != 0xFF),
            .includeRefScript = output_item->output_data.hasRefScript,
        };

        if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = output_item->output_data.destination.address.buffer;
            output_desc.destination.address.size = output_item->output_data.destination.address.size;
        } else {
            output_desc.destination.type = DESTINATION_DEVICE_OWNED;
            output_desc.destination.params = &output_item->output_data.destination.params;
        }

        security_policy_t policy = (output_desc.destination.type == DESTINATION_THIRD_PARTY)
            ? policyForSignTxOutputAddressBytes(
                &output_desc,
                tx->txSigningMode,
                tx->networkId,
                tx->protocolMagic,
                &G_context.tx_info.warning_bits)
            : policyForSignTxOutputAddressParams(
                &output_desc,
                tx->txSigningMode,
                tx->networkId,
                tx->protocolMagic,
                &G_context.tx_info.warning_bits);

        LEDGER_ASSERT(policy != POLICY_DENY, "Output denied during UI");

        switch (policy) {
            case POLICY_DENY:
                // Already asserted above, this case should never be reached
                break;
            case POLICY_SHOW: {
                char *output_num_tmp = ui_alloc_temp(MAX_UINT64_STRING_SIZE);
            if (output_num_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            snprintf(output_num_tmp, MAX_UINT64_STRING_SIZE, "#%d", output_num);
            status = ui_add_pair_or_fail("Output", output_num_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }

            char *address_tmp = ui_alloc_temp(MAX_HUMAN_ADDRESS_SIZE);
            if (address_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }

            bool address_formatted = false;
            if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
                address_formatted = format_address_human_readable(
                    output_item->output_data.destination.address.buffer,
                    output_item->output_data.destination.address.size,
                    address_tmp,
                    MAX_HUMAN_ADDRESS_SIZE
                );
            } else {
                uint8_t address_bytes[MAX_ADDRESS_SIZE];
                size_t derived_len = deriveAddress(
                    &output_item->output_data.destination.params,
                    address_bytes,
                    sizeof(address_bytes)
                );
                if (derived_len > 0) {
                    address_formatted = format_address_human_readable(
                        address_bytes,
                        derived_len,
                        address_tmp,
                        MAX_HUMAN_ADDRESS_SIZE
                    );
                }
            }

            if (!address_formatted) {
                return SWO_DISPLAY_ADDRESS_FAIL;
            }
            size_t address_len = strlen(address_tmp);
            if (address_len == 0 || address_len + 1 >= MAX_HUMAN_ADDRESS_SIZE) {
                return SWO_DISPLAY_ADDRESS_FAIL;
            }

            status = ui_add_pair_or_fail("Address", address_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }

            char *amount_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_SIZE);
            if (amount_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            bool amount_formatted = str_formatAdaAmount(output_item->output_data.adaAmount,
                                                        amount_tmp,
                                                        MAX_ADA_AMOUNT_STRING_SIZE);
            ASSERT(amount_formatted);
            status = ui_add_pair_or_fail("Amount", amount_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }

                output_num++;
            }
            break;
            case POLICY_HIDE:
                break;
        }

        if (output_item->output_data.assetGroups != NULL) {
            for (uint16_t ag = 0; ag < output_item->output_data.numAssetGroups; ag++) {
                if (output_item->output_data.assetGroups[ag].tokens != NULL) {
                    app_mem_free(output_item->output_data.assetGroups[ag].tokens);
                }
            }
            app_mem_free(output_item->output_data.assetGroups);
        }
        if (output_item->output_data.datum.type == DATUM_INLINE &&
            output_item->output_data.datum.inline_data.data != NULL) {
            app_mem_free(output_item->output_data.datum.inline_data.data);
        }
        if (output_item->output_data.hasRefScript && output_item->output_data.refScript.data != NULL) {
            app_mem_free(output_item->output_data.refScript.data);
        }
        app_mem_free(output_item);
        output_node = next;
    }
    tx->outputs = NULL;

    char *fee_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_SIZE);
    if (fee_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool fee_formatted = str_formatAdaAmount(tx->fee, fee_tmp, MAX_ADA_AMOUNT_STRING_SIZE);
    ASSERT(fee_formatted);
    status = ui_add_pair_or_fail("Fee", fee_tmp);
    if (status != SWO_SUCCESS) {
        return status;
    }

    if (tx->includeTtl) {
        security_policy_t ttl_policy = policyForSignTxTtl(tx->ttl);
        LEDGER_ASSERT(ttl_policy != POLICY_DENY, "TTL denied during UI");
        switch (ttl_policy) {
            case POLICY_DENY:
                // Already asserted above, this case should never be reached
                break;
            case POLICY_SHOW: {
                char *ttl_tmp = ui_alloc_temp(MAX_VALIDITY_BOUNDARY_STRING_SIZE);
                if (ttl_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                bool ttl_formatted = str_formatValidityBoundary(tx->ttl,
                                                                tx->networkId,
                                                                tx->protocolMagic,
                                                                ttl_tmp,
                                                                MAX_VALIDITY_BOUNDARY_STRING_SIZE);
                ASSERT(ttl_formatted);
                status = ui_add_pair_or_fail("TTL", ttl_tmp);
                if (status != SWO_SUCCESS) {
                    return status;
                }
                break;
            }
            case POLICY_HIDE:
                break;
        }
    }

    if (tx->includeValidityIntervalStart) {
        security_policy_t validity_interval_start_policy = policyForSignTxValidityIntervalStart();
        LEDGER_ASSERT(validity_interval_start_policy != POLICY_DENY, "Validity interval start denied during UI");
        switch (validity_interval_start_policy) {
            case POLICY_DENY:
                // Already asserted above, this case should never be reached
                break;
            case POLICY_SHOW: {
                char *validity_interval_start_tmp = ui_alloc_temp(MAX_VALIDITY_BOUNDARY_STRING_SIZE);
                if (validity_interval_start_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                bool vis_formatted = str_formatValidityBoundary(tx->validityIntervalStart,
                                                                tx->networkId,
                                                                tx->protocolMagic,
                                                                validity_interval_start_tmp,
                                                                MAX_VALIDITY_BOUNDARY_STRING_SIZE);
                ASSERT(vis_formatted);
                status = ui_add_pair_or_fail("Validity interval start", validity_interval_start_tmp);
                if (status != SWO_SUCCESS) {
                    return status;
                }
                break;
            }
            case POLICY_HIDE:
                break;
        }
    }

    uint16_t withdrawal_num = 1;
    s_flist_node *withdrawal_node = tx->withdrawals;
    while (withdrawal_node != NULL) {
        tx_withdrawal_list_item_t *withdrawal_item =
            (tx_withdrawal_list_item_t *) withdrawal_node;
        s_flist_node *next = withdrawal_node->next;

        security_policy_t policy = policyForSignTxWithdrawal(
            tx->txSigningMode,
            &withdrawal_item->withdrawal_data.stakeCredential,
            &G_context.tx_info.warning_bits
        );
        LEDGER_ASSERT(policy != POLICY_DENY, "Withdrawal denied during UI");

        switch (policy) {
            case POLICY_DENY:
                // Already asserted above, this case should never be reached
                break;
            case POLICY_SHOW: {
                char *withdrawal_num_tmp = ui_alloc_temp(MAX_UINT64_STRING_SIZE);
            if (withdrawal_num_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            snprintf(withdrawal_num_tmp, MAX_UINT64_STRING_SIZE, "#%d", withdrawal_num);
            status = ui_add_pair_or_fail("Withdrawal", withdrawal_num_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }

            char *withdrawal_amount_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_SIZE);
            if (withdrawal_amount_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            bool withdrawal_amount_formatted =
                str_formatAdaAmount(withdrawal_item->withdrawal_data.amount,
                                    withdrawal_amount_tmp,
                                    MAX_ADA_AMOUNT_STRING_SIZE);
            ASSERT(withdrawal_amount_formatted);
            status = ui_add_pair_or_fail("Amount", withdrawal_amount_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }

            char *reward_account_tmp = ui_alloc_temp(MAX_HUMAN_ADDRESS_SIZE);
            if (reward_account_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }

            uint8_t reward_addr_bytes[REWARD_ACCOUNT_SIZE];
            size_t reward_addr_len = 0;

            switch (withdrawal_item->withdrawal_data.stakeCredential.type) {
                case EXT_CREDENTIAL_KEY_PATH:
                    reward_addr_len = constructRewardAddressFromKeyPath(
                        &withdrawal_item->withdrawal_data.stakeCredential.keyPath,
                        G_context.tx_info.transaction.networkId,
                        reward_addr_bytes,
                        sizeof(reward_addr_bytes)
                    );
                    break;
                case EXT_CREDENTIAL_KEY_HASH:
                    reward_addr_len = constructRewardAddressFromHash(
                        G_context.tx_info.transaction.networkId,
                        REWARD_HASH_SOURCE_KEY,
                        withdrawal_item->withdrawal_data.stakeCredential.keyHash,
                        ADDRESS_KEY_HASH_LENGTH,
                        reward_addr_bytes,
                        sizeof(reward_addr_bytes)
                    );
                    break;
                case EXT_CREDENTIAL_SCRIPT_HASH:
                    reward_addr_len = constructRewardAddressFromHash(
                        G_context.tx_info.transaction.networkId,
                        REWARD_HASH_SOURCE_SCRIPT,
                        withdrawal_item->withdrawal_data.stakeCredential.scriptHash,
                        SCRIPT_HASH_LENGTH,
                        reward_addr_bytes,
                        sizeof(reward_addr_bytes)
                    );
                    break;
                default:
                    return SWO_TX_PARSING_FAIL;
            }

            if (reward_addr_len == 0) {
                return SWO_DISPLAY_ADDRESS_FAIL;
            }

            bool reward_formatted =
                format_address_human_readable(reward_addr_bytes,
                                              reward_addr_len,
                                              reward_account_tmp,
                                              MAX_HUMAN_ADDRESS_SIZE);
            ASSERT(reward_formatted);
            size_t reward_display_len = strlen(reward_account_tmp);
            if (reward_display_len == 0 || reward_display_len + 1 >= MAX_HUMAN_ADDRESS_SIZE) {
                return SWO_DISPLAY_ADDRESS_FAIL;
            }

            status = ui_add_pair_or_fail("Reward account", reward_account_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }

                withdrawal_num++;
            }
            break;
            case POLICY_HIDE:
                break;
        }

        app_mem_free(withdrawal_item);
        withdrawal_node = next;
    }
    tx->withdrawals = NULL;

    if (tx->num_mint_asset_groups > 0) {
        security_policy_t mint_policy = policyForSignTxMintInit(tx->txSigningMode);
        LEDGER_ASSERT(mint_policy != POLICY_DENY, "Mint denied during UI");
        if (mint_policy == POLICY_SHOW) {
            char *summary_tmp = ui_alloc_temp(MAX_MINT_SUMMARY_STRING_SIZE);
            if (summary_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            snprintf(summary_tmp,
                     MAX_MINT_SUMMARY_STRING_SIZE,
                     "%u asset group%s",
                     tx->num_mint_asset_groups,
                     (tx->num_mint_asset_groups == 1) ? "" : "s");
            status = ui_add_pair_or_fail("Mint", summary_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }

            token_group_t tokenGroup;
            s_flist_node *mint_node = tx->mint_asset_groups;
            while (mint_node != NULL) {
                mint_asset_group_list_item_t *item =
                    (mint_asset_group_list_item_t *) mint_node;
                memcpy(tokenGroup.policyId,
                       item->asset_group.policyId,
                       sizeof(tokenGroup.policyId));

                if (item->asset_group.tokens == NULL) {
                    mint_node = mint_node->next;
                    continue;
                }

                for (uint16_t tk = 0; tk < item->asset_group.numTokens; tk++) {
                    mint_token_t *token = &item->asset_group.tokens[tk];

                    char *fingerprint_tmp = ui_alloc_temp(MAX_TOKEN_FINGERPRINT_STRING_SIZE);
                    if (fingerprint_tmp == NULL) {
                        return SWO_INSUFFICIENT_MEMORY;
                    }
                    size_t fingerprint_len = deriveAssetFingerprintBech32(
                        tokenGroup.policyId,
                        sizeof(tokenGroup.policyId),
                        token->assetName,
                        token->assetNameLen,
                        fingerprint_tmp,
                        MAX_TOKEN_FINGERPRINT_STRING_SIZE);
                    if (fingerprint_len == 0) {
                        return SWO_DISPLAY_ADDRESS_FAIL;
                    }
                    status = ui_add_pair_or_fail("Mint fingerprint", fingerprint_tmp);
                    if (status != SWO_SUCCESS) {
                        return status;
                    }

                    char *amount_tmp = ui_alloc_temp(MAX_MINT_AMOUNT_STRING_SIZE);
                    if (amount_tmp == NULL) {
                        return SWO_INSUFFICIENT_MEMORY;
                    }
                    bool mint_amount_formatted = str_formatTokenAmountMint(&tokenGroup,
                                                                           token->assetName,
                                                                           token->assetNameLen,
                                                                           token->amount,
                                                                           amount_tmp,
                                                                           MAX_MINT_AMOUNT_STRING_SIZE);
                    ASSERT(mint_amount_formatted);
                    status = ui_add_pair_or_fail("Mint amount", amount_tmp);
                    if (status != SWO_SUCCESS) {
                        return status;
                    }
                }

                mint_node = mint_node->next;
            }
        }

        security_policy_t mint_confirm_policy =
            policyForSignTxMintConfirm(mint_policy);
        LEDGER_ASSERT(mint_confirm_policy != POLICY_DENY, "Mint confirm denied during UI");
    }

    s_flist_node *input_node = tx->inputs;
    while (input_node != NULL) {
        s_flist_node *next = input_node->next;
        app_mem_free(input_node);
        input_node = next;
    }
    tx->inputs = NULL;

    s_flist_node *mint_node = tx->mint_asset_groups;
    while (mint_node != NULL) {
        mint_asset_group_list_item_t *item = (mint_asset_group_list_item_t *) mint_node;
        s_flist_node *next = mint_node->next;
        if (item->asset_group.tokens != NULL) {
            app_mem_free(item->asset_group.tokens);
        }
        app_mem_free(mint_node);
        mint_node = next;
    }
    tx->mint_asset_groups = NULL;

    if (G_context.tx_info.raw_tx != NULL) {
        app_mem_free(G_context.tx_info.raw_tx);
        G_context.tx_info.raw_tx = NULL;
        G_context.tx_info.raw_tx_len = 0;
    }

    char *hash_tmp = ui_alloc_temp(MAX_TX_HASH_DISPLAY_SIZE);
    if (hash_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    if (bytes_to_lowercase_hex(hash_tmp,
                               MAX_TX_HASH_DISPLAY_SIZE,
                               G_context.tx_info.tx_hash,
                               sizeof(G_context.tx_info.tx_hash)) != 0) {
        return SWO_DISPLAY_ADDRESS_FAIL;
    }
    status = ui_add_pair_or_fail("Transaction hash", hash_tmp);
    if (status != SWO_SUCCESS) {
        return status;
    }

    return SWO_SUCCESS;
}

static int ui_build_transaction_warnings(void) {
    const warning_definition_t *warning_defs[WARNING_BIT_COUNT];
    size_t warning_count =
        warning_bits_to_definitions(G_context.tx_info.warning_bits, warning_defs, WARNING_BIT_COUNT);
    if (warning_count == 0) {
        g_warning = NULL;
        return SWO_SUCCESS;
    }

    const nbgl_icon_details_t **icons =
        (const nbgl_icon_details_t **) ui_mem_alloc(sizeof(nbgl_icon_details_t *) * warning_count);
    const char **titles = (const char **) ui_mem_alloc(sizeof(const char *) * warning_count);
    const char **subtexts = (const char **) ui_mem_alloc(sizeof(const char *) * warning_count);
    nbgl_warningDetails_t *details =
        (nbgl_warningDetails_t *) ui_mem_alloc(sizeof(nbgl_warningDetails_t) * warning_count);
    nbgl_warningDetails_t *intro = (nbgl_warningDetails_t *) ui_mem_alloc(sizeof(nbgl_warningDetails_t));
    nbgl_warningDetails_t *review = (nbgl_warningDetails_t *) ui_mem_alloc(sizeof(nbgl_warningDetails_t));
    nbgl_contentCenter_t *info = (nbgl_contentCenter_t *) ui_mem_alloc(sizeof(nbgl_contentCenter_t));
    g_warning = (nbgl_warning_t *) ui_mem_alloc(sizeof(nbgl_warning_t));

    if (icons == NULL || titles == NULL || subtexts == NULL || details == NULL || intro == NULL ||
        review == NULL || info == NULL || g_warning == NULL) {
        g_warning = NULL;
        return SWO_INSUFFICIENT_MEMORY;
    }

    for (size_t i = 0; i < warning_count; i++) {
        const warning_definition_t *def = (const warning_definition_t *) PIC(warning_defs[i]);
        const char *title = (const char *) PIC(def->title);
        const char *description = (const char *) PIC(def->description);
        titles[i] = title;
        subtexts[i] = description;
        icons[i] = &WARNING_ICON;

        details[i].title = title;
        details[i].type = CENTERED_INFO_WARNING;
        details[i].centeredInfo.icon = &WARNING_ICON;
        details[i].centeredInfo.title = title;
        details[i].centeredInfo.description = description;
    }

    const char *const warning_intro_title = (const char *) PIC("Security report");
    const char *const warning_review_title = (const char *) PIC("Warning details");
    const char *const warning_info_title = (const char *) PIC("Transaction warning");
    const char *const warning_info_desc = (const char *) PIC("Please review the security warnings before signing.");

    intro->title = warning_intro_title;
    intro->type = BAR_LIST_WARNING;
    intro->barList.nbBars = warning_count;
    intro->barList.icons = icons;
    intro->barList.texts = titles;
    intro->barList.subTexts = subtexts;
    intro->barList.details = details;

    review->title = warning_review_title;
    review->type = BAR_LIST_WARNING;
    review->barList.nbBars = warning_count;
    review->barList.icons = icons;
    review->barList.texts = titles;
    review->barList.subTexts = subtexts;
    review->barList.details = details;

    info->icon = &WARNING_ICON;
    info->title = warning_info_title;
    info->description = warning_info_desc;

    g_warning->introDetails = intro;
    g_warning->reviewDetails = review;
    g_warning->info = info;
    g_warning->introTopRightIcon = &WARNING_ICON;
    g_warning->reviewTopRightIcon = &WARNING_ICON;

    return SWO_SUCCESS;
}

static inline bool status_requires_streaming(int status) {
    return status == SWO_INSUFFICIENT_MEMORY;
}

static int ui_build_pairs_and_warnings(void) {
    int status = ui_materialize_strings();
    if (status != SWO_SUCCESS) {
        return status;
    }
    return ui_build_transaction_warnings();
}

int ui_prepare_transaction_review(void) {
    if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
        return send_error_and_reset(SWO_BAD_STATE);
    }
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_HASHED, "UI prep called too early");
    uint16_t pair_count = G_context.tx_info.planned_ui_pairs;
    if (pair_count == 0 || pair_count > UINT8_MAX) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL);
    }
    if (!ui_pairs_init((uint8_t) pair_count)) {
        LEDGER_ASSERT(false, "Need streaming UI but not implemented (pair count %u)", (uint32_t) pair_count);
        return SWO_DISPLAY_AMOUNT_FAIL;
    }

    int status = ui_build_pairs_and_warnings();
    if (status != SWO_SUCCESS) {
        ui_pairs_cleanup();
        ui_clear_prepared_warning();
        if (status_requires_streaming(status)) {
            LEDGER_ASSERT(false, "Need streaming UI but not implemented (status=0x%04x)", status);
        }
        return status;
    }
    return SWO_SUCCESS;
}

const nbgl_warning_t *ui_get_prepared_warning(void) {
    return g_warning;
}

void ui_clear_prepared_warning(void) {
    g_warning = NULL;
}
