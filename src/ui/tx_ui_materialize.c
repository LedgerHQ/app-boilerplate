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
#include "ui/tx_ui_helpers.h"
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

    TRACE("UI materialization starting");

    uint16_t output_num = 1;
    s_flist_node *output_node = tx->outputs;
    TRACE("Materializing %u outputs", tx->num_outputs);
    while (output_node != NULL) {
        tx_output_list_item_t *output_item = (tx_output_list_item_t *) output_node;
        s_flist_node *next = output_node->next;

        tx_output_description_t output_desc = {
            .format = output_item->output_data.format,
            .amount = output_item->output_data.adaAmount,
            .numAssetGroups = output_item->output_data.numAssetGroups,
            .includeDatum = output_item->output_data.datum.hasDatum,
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
                TRACE("Materializing output #%u", output_num);
                char *output_num_tmp = ui_alloc_temp(MAX_UINT64_STRING_LENGTH);
                if (output_num_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                snprintf(output_num_tmp, MAX_UINT64_STRING_LENGTH, "#%d", output_num);
                status = ui_add_pair_or_fail("Output", output_num_tmp);
                if (status != SWO_SUCCESS) {
                    return status;
                }

                char *address_tmp = ui_alloc_temp(MAX_HUMAN_ADDRESS_LENGTH);
                if (address_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }

                bool address_formatted = false;
                if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
                    address_formatted = format_address_human_readable(
                        output_item->output_data.destination.address.buffer,
                        output_item->output_data.destination.address.size,
                        address_tmp,
                        MAX_HUMAN_ADDRESS_LENGTH
                    );
                } else {
                    uint8_t address_bytes[MAX_ADDRESS_LENGTH];
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
                            MAX_HUMAN_ADDRESS_LENGTH
                        );
                    }
                }

                LEDGER_ASSERT(address_formatted, "Address formatting failed");
                size_t address_len = strlen(address_tmp);
                LEDGER_ASSERT(address_len > 0, "Address length zero");
                LEDGER_ASSERT(address_len + 1 < MAX_HUMAN_ADDRESS_LENGTH, "Address truncated");

                status = ui_add_pair_or_fail("Address", address_tmp);
                if (status != SWO_SUCCESS) {
                    return status;
                }

                char *amount_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH);
                if (amount_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                bool amount_formatted = str_formatAdaAmount(output_item->output_data.adaAmount,
                                                            amount_tmp,
                                                            MAX_ADA_AMOUNT_STRING_LENGTH);
                ASSERT(amount_formatted);
                status = ui_add_pair_or_fail("Amount", amount_tmp);
                if (status != SWO_SUCCESS) {
                    return status;
                }

                // Display tokens if output is shown
                if (output_item->output_data.assetGroups != NULL) {
                    for (uint16_t ag = 0; ag < output_item->output_data.numAssetGroups; ag++) {
                        asset_group_t *group = &output_item->output_data.assetGroups[ag];

                        // Iterate through linked list of tokens
                        s_flist_node *token_node = group->tokens;
                        while (token_node != NULL) {
                            output_token_list_item_t *token_item = (output_token_list_item_t *) token_node;
                            output_token_t *token = &token_item->token_data;
                            s_flist_node *token_next = token_node->next;

                            // Display token fingerprint
                            char *fingerprint_tmp = ui_alloc_temp(MAX_TOKEN_FINGERPRINT_STRING_LENGTH);
                            if (fingerprint_tmp == NULL) {
                                return SWO_INSUFFICIENT_MEMORY;
                            }
                            size_t fingerprint_len = deriveAssetFingerprintBech32(
                                group->policyId,
                                MINTING_POLICY_ID_LENGTH,
                                token->assetName,
                                token->assetNameLen,
                                fingerprint_tmp,
                                MAX_TOKEN_FINGERPRINT_STRING_LENGTH);
                            LEDGER_ASSERT(fingerprint_len > 0, "Fingerprint derivation failed");
                            status = ui_add_pair_or_fail("Asset fingerprint", fingerprint_tmp);
                            if (status != SWO_SUCCESS) {
                                return status;
                            }

                            // Display token amount
                            char *token_amount_tmp = ui_alloc_temp(MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH);
                            if (token_amount_tmp == NULL) {
                                return SWO_INSUFFICIENT_MEMORY;
                            }
                            token_group_t tokenGroup;
                            memcpy(tokenGroup.policyId, group->policyId, MINTING_POLICY_ID_LENGTH);
                            bool token_amount_formatted = str_formatTokenAmountOutput(
                                &tokenGroup,
                                token->assetName,
                                token->assetNameLen,
                                token->amount,
                                token_amount_tmp,
                                MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH);
                            ASSERT(token_amount_formatted);
                            status = ui_add_pair_or_fail("Token amount", token_amount_tmp);
                            if (status != SWO_SUCCESS) {
                                return status;
                            }

                            // Free token node immediately after UI strings are materialized
                            app_mem_free(token_node);
                            token_node = token_next;
                        }
                    }
                }

                output_num++;
            }
            break;
            case POLICY_HIDE:
                break;
        }

        if (output_item->output_data.assetGroups != NULL) {
            for (uint16_t ag = 0; ag < output_item->output_data.numAssetGroups; ag++) {
                // Token nodes are already freed during UI materialization
                // Only free the asset group array itself
            }
            app_mem_free(output_item->output_data.assetGroups);
        }
        // Note: inline datum and reference script data are pointers into the raw_tx buffer,
        // not separately allocated, so they do not need to be freed
        app_mem_free(output_item);
        output_node = next;
    }
    tx->outputs = NULL;

    char *fee_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH);
    if (fee_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool fee_formatted = str_formatAdaAmount(tx->fee, fee_tmp, MAX_ADA_AMOUNT_STRING_LENGTH);
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
                char *ttl_tmp = ui_alloc_temp(MAX_VALIDITY_BOUNDARY_STRING_LENGTH);
                if (ttl_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                bool ttl_formatted = str_formatValidityBoundary(tx->ttl,
                                                                tx->networkId,
                                                                tx->protocolMagic,
                                                                ttl_tmp,
                                                                MAX_VALIDITY_BOUNDARY_STRING_LENGTH);
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
                char *validity_interval_start_tmp = ui_alloc_temp(MAX_VALIDITY_BOUNDARY_STRING_LENGTH);
                if (validity_interval_start_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                bool vis_formatted = str_formatValidityBoundary(tx->validityIntervalStart,
                                                                tx->networkId,
                                                                tx->protocolMagic,
                                                                validity_interval_start_tmp,
                                                                MAX_VALIDITY_BOUNDARY_STRING_LENGTH);
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

    // Display certificates
    uint16_t certificate_num = 1;
    s_flist_node *certificate_node = tx->certificates;
    TRACE("Materializing %u certificates", tx->num_certificates);
    while (certificate_node != NULL) {
        tx_certificate_list_item_t *certificate_item =
            (tx_certificate_list_item_t *) certificate_node;
        s_flist_node *next = certificate_node->next;

        // Determine security policy based on certificate type
        security_policy_t policy;
        switch (certificate_item->certificate_data.type) {
            case CERTIFICATE_STAKE_REGISTRATION:
            case CERTIFICATE_STAKE_DEREGISTRATION:
            case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
            case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY:
            case CERTIFICATE_STAKE_DELEGATION:
                policy = policyForSignTxCertificateStaking(
                    tx->txSigningMode,
                    certificate_item->certificate_data.type,
                    &certificate_item->certificate_data.stakeCredential
                );
                break;

            case CERTIFICATE_VOTE_DELEGATION:
                policy = policyForSignTxCertificateVoteDelegation(
                    tx->txSigningMode,
                    &certificate_item->certificate_data.stakeCredential,
                    &certificate_item->certificate_data.drep
                );
                break;

            case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT:
                policy = policyForSignTxCertificateCommitteeAuth(
                    tx->txSigningMode,
                    &certificate_item->certificate_data.coldCredential,
                    &certificate_item->certificate_data.hotCredential
                );
                break;

            case CERTIFICATE_RESIGN_COMMITTEE_COLD:
                policy = policyForSignTxCertificateCommitteeResign(
                    tx->txSigningMode,
                    &certificate_item->certificate_data.coldCredential
                );
                break;

            case CERTIFICATE_DREP_REGISTRATION:
            case CERTIFICATE_DREP_DEREGISTRATION:
            case CERTIFICATE_DREP_UPDATE:
                policy = policyForSignTxCertificateDRep(
                    tx->txSigningMode,
                    &certificate_item->certificate_data.dRepCredential
                );
                break;

            case CERTIFICATE_STAKE_POOL_RETIREMENT:
                policy = policyForSignTxCertificateStakePoolRetirement(
                    tx->txSigningMode,
                    &certificate_item->certificate_data.poolCredential,
                    certificate_item->certificate_data.retirementEpoch
                );
                break;

            default:
                LEDGER_ASSERT(false, "Unknown certificate type");
                policy = POLICY_DENY;
                break;
        }

        switch (policy) {
            case POLICY_DENY:
                LEDGER_ASSERT(false, "Certificate denied during UI");
                break;
            case POLICY_SHOW: {
                TRACE("Materializing certificate #%u type=%u", certificate_num, certificate_item->certificate_data.type);
                char *cert_num_tmp = ui_alloc_temp(MAX_UINT64_STRING_LENGTH);
                if (cert_num_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                snprintf(cert_num_tmp, MAX_UINT64_STRING_LENGTH, "#%d", certificate_num);
                status = ui_add_pair_or_fail("Certificate", cert_num_tmp);
                if (status != SWO_SUCCESS) {
                    return status;
                }

                // Certificate type
                const char *cert_type_name = getCertificateTypeName(certificate_item->certificate_data.type);
                size_t cert_type_len = strlen(cert_type_name);
                char *cert_type_tmp = ui_alloc_temp(cert_type_len + 1);
                if (cert_type_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                memcpy(cert_type_tmp, cert_type_name, cert_type_len + 1);
                status = ui_add_pair_or_fail("Type", cert_type_tmp);
                if (status != SWO_SUCCESS) {
                    return status;
                }

                // Certificate-specific fields
                switch (certificate_item->certificate_data.type) {
                    case CERTIFICATE_STAKE_REGISTRATION:
                    case CERTIFICATE_STAKE_DEREGISTRATION: {
                        // Display stake credential
                        status = displayCredential(
                            &certificate_item->certificate_data.stakeCredential,
                            "Stake key",           // KEY_PATH label
                            "Stake key hash",      // KEY_HASH label
                            "stake_vkh",           // KEY_HASH bech32 prefix
                            "Stake script hash",   // SCRIPT_HASH label
                            "script"               // SCRIPT_HASH bech32 prefix
                        );
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_STAKE_DELEGATION: {
                        // Display stake credential
                        status = displayCredential(
                            &certificate_item->certificate_data.stakeCredential,
                            "Stake key",           // KEY_PATH label
                            "Stake key hash",      // KEY_HASH label
                            "stake_vkh",           // KEY_HASH bech32 prefix
                            "Stake script hash",   // SCRIPT_HASH label
                            "script"               // SCRIPT_HASH bech32 prefix
                        );
                        if (status != SWO_SUCCESS) {
                            return status;
                        }

                        // Display pool key hash
                        status = displayPoolKeyHash(
                            certificate_item->certificate_data.poolKeyHash,
                            "Pool"
                        );
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
                    case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY: {
                        // Display stake credential
                        status = displayCredential(
                            &certificate_item->certificate_data.stakeCredential,
                            "Stake key",           // KEY_PATH label
                            "Stake key hash",      // KEY_HASH label
                            "stake_vkh",           // KEY_HASH bech32 prefix
                            "Stake script hash",   // SCRIPT_HASH label
                            "script"               // SCRIPT_HASH bech32 prefix
                        );
                        if (status != SWO_SUCCESS) {
                            return status;
                        }

                        // Display deposit
                        status = displayDeposit(certificate_item->certificate_data.deposit, "Deposit");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_STAKE_POOL_RETIREMENT: {
                        // Display pool credential
                        const ext_credential_t* poolCred = &certificate_item->certificate_data.poolCredential;
                        uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH];

                        switch (poolCred->type) {
                            case EXT_CREDENTIAL_KEY_PATH:
                                bip44_pathToKeyHash(&poolCred->keyPath, poolKeyHash, sizeof(poolKeyHash));
                                break;
                            case EXT_CREDENTIAL_KEY_HASH: {
                                STATIC_ASSERT(ADDRESS_KEY_HASH_LENGTH == POOL_KEY_HASH_LENGTH,
                                              "pool credential hash size mismatch");
                                memcpy(poolKeyHash, poolCred->keyHash, POOL_KEY_HASH_LENGTH);
                                break;
                            }
                            default:
                                LEDGER_ASSERT(false, "Unsupported pool credential type for retirement");
                                return SWO_TX_PARSING_FAIL;
                        }

                        // Display pool key hash with "pool" prefix
                        status = displayPoolKeyHash(poolKeyHash, "Pool ID");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }

                        // Display retirement epoch
                        char *epoch_tmp = ui_alloc_temp(MAX_UINT64_STRING_LENGTH);
                        if (epoch_tmp == NULL) {
                            return SWO_INSUFFICIENT_MEMORY;
                        }
                        snprintf(epoch_tmp, MAX_UINT64_STRING_LENGTH, "%llu",
                                (unsigned long long) certificate_item->certificate_data.retirementEpoch);
                        status = ui_add_pair_or_fail("Retirement epoch", epoch_tmp);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_VOTE_DELEGATION: {
                        // Display stake credential
                        status = displayCredential(
                            &certificate_item->certificate_data.stakeCredential,
                            "Stake key",           // KEY_PATH label
                            "Stake key hash",      // KEY_HASH label
                            "stake_vkh",           // KEY_HASH bech32 prefix
                            "Stake script hash",   // SCRIPT_HASH label
                            "script"               // SCRIPT_HASH bech32 prefix
                        );
                        if (status != SWO_SUCCESS) {
                            return status;
                        }

                        // Display DRep
                        const ext_drep_t* drep = &certificate_item->certificate_data.drep;
                        status = displayDRep(drep, "DRep");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT: {
                        // Display cold credential
                        const ext_credential_t* coldCred = &certificate_item->certificate_data.coldCredential;
                        status = displayCredential(coldCred,
                                                  "Committee cold key",
                                                  "Committee cold key hash",
                                                  "cc_cold",
                                                  "Committee cold script hash",
                                                  "cc_cold_script");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }

                        // Display hot credential
                        const ext_credential_t* hotCred = &certificate_item->certificate_data.hotCredential;
                        status = displayCredential(hotCred,
                                                  "Committee hot key",
                                                  "Committee hot key hash",
                                                  "cc_hot",
                                                  "Committee hot script hash",
                                                  "cc_hot_script");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_RESIGN_COMMITTEE_COLD: {
                        // Display cold credential
                        const ext_credential_t* coldCred = &certificate_item->certificate_data.coldCredential;
                        status = displayCredential(coldCred,
                                                  "Committee cold key",
                                                  "Committee cold key hash",
                                                  "cc_cold",
                                                  "Committee cold script hash",
                                                  "cc_cold");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }

                        // Display anchor if present
                        status = displayAnchorIfPresent(&certificate_item->certificate_data.anchor);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_DREP_REGISTRATION: {
                        // Display DRep credential
                        const ext_credential_t* drepCred = &certificate_item->certificate_data.dRepCredential;
                        status = displayCredential(drepCred,
                                                  "DRep key",
                                                  "DRep key hash",
                                                  "drep",
                                                  "DRep script hash",
                                                  "drep");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }

                        // Display deposit
                        status = displayDeposit(certificate_item->certificate_data.deposit, "Deposit");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }

                        // Display anchor if present
                        status = displayAnchorIfPresent(&certificate_item->certificate_data.anchor);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_DREP_DEREGISTRATION: {
                        // Display DRep credential
                        const ext_credential_t* drepCred = &certificate_item->certificate_data.dRepCredential;
                        status = displayCredential(drepCred,
                                                  "DRep key",
                                                  "DRep key hash",
                                                  "drep",
                                                  "DRep script hash",
                                                  "drep");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }

                        // Display deposit
                        status = displayDeposit(certificate_item->certificate_data.deposit, "Deposit");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_DREP_UPDATE: {
                        // Display DRep credential
                        const ext_credential_t* drepCred = &certificate_item->certificate_data.dRepCredential;
                        status = displayCredential(drepCred,
                                                  "DRep key",
                                                  "DRep key hash",
                                                  "drep",
                                                  "DRep script hash",
                                                  "drep");
                        if (status != SWO_SUCCESS) {
                            return status;
                        }

                        // Display anchor if present
                        status = displayAnchorIfPresent(&certificate_item->certificate_data.anchor);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    default:
                        return SWO_TX_PARSING_FAIL;
                }

                certificate_num++;
            }
            break;
            case POLICY_HIDE:
                break;
        }

        app_mem_free(certificate_item);
        certificate_node = next;
    }
    tx->certificates = NULL;

    uint16_t withdrawal_num = 1;
    s_flist_node *withdrawal_node = tx->withdrawals;
    TRACE("Materializing %u withdrawals", tx->num_withdrawals);
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
                TRACE("Materializing withdrawal #%u", withdrawal_num);
                char *withdrawal_num_tmp = ui_alloc_temp(MAX_UINT64_STRING_LENGTH);
            if (withdrawal_num_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            snprintf(withdrawal_num_tmp, MAX_UINT64_STRING_LENGTH, "#%d", withdrawal_num);
            status = ui_add_pair_or_fail("Withdrawal", withdrawal_num_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }

            char *withdrawal_amount_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH);
            if (withdrawal_amount_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            bool withdrawal_amount_formatted =
                str_formatAdaAmount(withdrawal_item->withdrawal_data.amount,
                                    withdrawal_amount_tmp,
                                    MAX_ADA_AMOUNT_STRING_LENGTH);
            ASSERT(withdrawal_amount_formatted);
            status = ui_add_pair_or_fail("Amount", withdrawal_amount_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }

            // Display reward account with proper formatting
            // For KEY_PATH: shows "Reward account #N" with "path address"
            // For KEY_HASH/SCRIPT_HASH: shows "Reward account" with just address
            status = displayRewardAccountFromCredential(
                G_context.tx_info.transaction.networkId,
                &withdrawal_item->withdrawal_data.stakeCredential
            );
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
            char *summary_tmp = ui_alloc_temp(MAX_MINT_SUMMARY_STRING_LENGTH);
            if (summary_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            snprintf(summary_tmp,
                     MAX_MINT_SUMMARY_STRING_LENGTH,
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

                // Iterate through linked list of tokens
                s_flist_node *token_node = item->asset_group.tokens;
                while (token_node != NULL) {
                    mint_token_list_item_t *token_item = (mint_token_list_item_t *) token_node;
                    mint_token_t *token = &token_item->token_data;
                    s_flist_node *token_next = token_node->next;

                    char *fingerprint_tmp = ui_alloc_temp(MAX_TOKEN_FINGERPRINT_STRING_LENGTH);
                    if (fingerprint_tmp == NULL) {
                        return SWO_INSUFFICIENT_MEMORY;
                    }
                    size_t fingerprint_len = deriveAssetFingerprintBech32(
                        tokenGroup.policyId,
                        sizeof(tokenGroup.policyId),
                        token->assetName,
                        token->assetNameLen,
                        fingerprint_tmp,
                        MAX_TOKEN_FINGERPRINT_STRING_LENGTH);
                    LEDGER_ASSERT(fingerprint_len > 0, "Fingerprint derivation failed");
                    status = ui_add_pair_or_fail("Mint fingerprint", fingerprint_tmp);
                    if (status != SWO_SUCCESS) {
                        return status;
                    }

                    char *amount_tmp = ui_alloc_temp(MAX_MINT_AMOUNT_STRING_LENGTH);
                    if (amount_tmp == NULL) {
                        return SWO_INSUFFICIENT_MEMORY;
                    }
                    bool mint_amount_formatted = str_formatTokenAmountMint(&tokenGroup,
                                                                           token->assetName,
                                                                           token->assetNameLen,
                                                                           token->amount,
                                                                           amount_tmp,
                                                                           MAX_MINT_AMOUNT_STRING_LENGTH);
                    ASSERT(mint_amount_formatted);
                    status = ui_add_pair_or_fail("Mint amount", amount_tmp);
                    if (status != SWO_SUCCESS) {
                        return status;
                    }

                    // Free token node immediately after UI strings are materialized
                    app_mem_free(token_node);
                    token_node = token_next;
                }

                mint_node = mint_node->next;
            }
        }
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
        s_flist_node *next = mint_node->next;

        // Token nodes are already freed during UI materialization
        // Only free the asset group node itself
        app_mem_free(mint_node);
        mint_node = next;
    }
    tx->mint_asset_groups = NULL;

    if (G_context.tx_info.raw_tx != NULL) {
        app_mem_free(G_context.tx_info.raw_tx);
        G_context.tx_info.raw_tx = NULL;
        G_context.tx_info.raw_tx_len = 0;
    }

    char *hash_tmp = ui_alloc_temp(MAX_TX_HASH_DISPLAY_LENGTH);
    if (hash_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    int hex_status = bytes_to_lowercase_hex(hash_tmp,
                                            MAX_TX_HASH_DISPLAY_LENGTH,
                                            G_context.tx_info.tx_hash,
                                            sizeof(G_context.tx_info.tx_hash));
    LEDGER_ASSERT(hex_status == 0, "Tx hash hex formatting failed");
    status = ui_add_pair_or_fail("Transaction hash", hash_tmp);
    if (status != SWO_SUCCESS) {
        return status;
    }

    TRACE("UI materialization complete");
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
