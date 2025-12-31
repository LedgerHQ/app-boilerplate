#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "addressUtils/addressUtilsShelley.h"
#include "addressUtils/bip44.h"
#include "auxDataHashBuilder/auxDataHashBuilder.h"
#include "messageSigning/cip8_types.h"
#include "securityPolicyType.h"
#include "transaction/tx_aux_data_types.h"
#include "transaction/tx_hash_builder.h"
#include "transaction/tx.h"

#include "securityWarnings.h"

typedef struct {
    warning_bit_e bit;
    const char* title;
    const char* description;
} warning_definition_t;

size_t warning_bits_to_definitions(warning_bits_t warnings,
                                   const warning_definition_t** definitions,
                                   size_t max_definitions);

security_policy_t policyForDerivePrivateKey(const bip44_path_t* path);

security_policy_t policyForGetExtendedPublicKey(const bip44_path_t* path, warning_bits_t* warnings);

security_policy_t policyForShowDeriveAddress(const addressParams_t* addressParams);
security_policy_t policyForReturnDeriveAddress(const addressParams_t* addressParams);

security_policy_t policyForSignTxFee(sign_tx_signingmode_t txSigningMode,
                                     uint64_t fee,
                                     warning_bits_t* warnings);

bool isNetworkUsual(uint32_t networkId, uint32_t protocolMagic);
bool isTxNetworkIdVerifiable(bool includeNetworkId,
                             uint32_t numOutputs,
                             uint32_t numWithdrawals,
                             sign_tx_signingmode_t txSigningMode);
bool needsRunningScriptWarning(int32_t numCollateralInputs);
bool needsMissingCollateralWarning(sign_tx_signingmode_t signingMode, uint32_t numCollateralInputs);
bool needsUnknownCollateralWarning(sign_tx_signingmode_t signingMode, bool includesTotalCollateral);
bool needsMissingScriptDataHashWarning(sign_tx_signingmode_t signingMode,
                                       bool includesScriptDataHash);

security_policy_t policyForSignTxInit(sign_tx_signingmode_t txSigningMode,
                                      uint32_t networkId,
                                      uint32_t protocolMagic,
                                      uint16_t numOutputs,
                                      uint16_t numCertificates,
                                      uint16_t numWithdrawals,
                                      bool includeMint,
                                      bool includeScriptDataHash,
                                      uint16_t numCollateralInputs,
                                      uint16_t numRequiredSigners,
                                      bool includeNetworkId,
                                      bool includeCollateralOutput,
                                      bool includeTotalCollateral,
                                      uint16_t numReferenceInputs,
                                      uint16_t numVotingProcedures,
                                      bool includeTreasury,
                                      bool includeDonation,
                                      warning_bits_t* warnings);

security_policy_t policyForSignTxInput(sign_tx_signingmode_t txSigningMode);

bool needsMissingDatumWarning(const tx_output_destination_t* destination, bool includeDatum);

security_policy_t policyForSignTxOutputAddressBytes(const tx_output_description_t* output,
                                                    sign_tx_signingmode_t txSigningMode,
                                                    const uint8_t networkId,
                                                    const uint32_t protocolMagic,
                                                    warning_bits_t* warnings);
security_policy_t policyForSignTxOutputAddressParams(const tx_output_description_t* output,
                                                     sign_tx_signingmode_t txSigningMode,
                                                     const uint8_t networkId,
                                                     const uint32_t protocolMagic,
                                                     warning_bits_t* warnings);
security_policy_t policyForSignTxOutputDatumHash(security_policy_t outputPolicy);

security_policy_t policyForSignTxOutputRefScript(security_policy_t outputPolicy);

security_policy_t policyForSignTxOutputConfirm(security_policy_t addressPolicy,
                                               uint64_t numAssetGroups,
                                               bool containsDatum,
                                               bool containsRefScript);

security_policy_t policyForSignTxCollateralOutputAddressBytes(const tx_output_description_t* output,
                                                              sign_tx_signingmode_t txSigningMode,
                                                              const uint8_t networkId,
                                                              const uint32_t protocolMagic);
security_policy_t policyForSignTxCollateralOutputAddressParams(
    const tx_output_description_t* output,
    sign_tx_signingmode_t txSigningMode,
    const uint8_t networkId,
    const uint32_t protocolMagic,
    bool isTotalCollateralIncluded);
security_policy_t policyForSignTxCollateralOutputAdaAmount(security_policy_t outputPolicy,
                                                           bool isTotalCollateralPresent);
security_policy_t policyForSignTxCollateralOutputTokens(security_policy_t outputPolicy,
                                                        const tx_output_description_t* output);
security_policy_t policyForSignTxCollateralOutputConfirm(security_policy_t outputPolicy,
                                                         uint64_t numAssetGroups);

security_policy_t policyForSignTxTtl(uint32_t ttl);
security_policy_t policyForSignTxCertificate(sign_tx_signingmode_t txSigningMode,
                                             const certificate_type_t certificateType);
security_policy_t policyForSignTxCertificateStaking(sign_tx_signingmode_t txSigningMode,
                                                    const certificate_type_t certificateType,
                                                    const ext_credential_t* stakeCredential);
security_policy_t policyForSignTxCertificateVoteDelegation(sign_tx_signingmode_t txSigningMode,
                                                           const ext_credential_t* stakeCredential,
                                                           const ext_drep_t* drep);
security_policy_t policyForSignTxCertificateCommitteeAuth(sign_tx_signingmode_t txSigningMode,
                                                          const ext_credential_t* coldCredential,
                                                          const ext_credential_t* hotCredential);
security_policy_t policyForSignTxCertificateCommitteeResign(sign_tx_signingmode_t txSigningMode,
                                                            const ext_credential_t* coldCredential);
security_policy_t policyForSignTxCertificateDRep(sign_tx_signingmode_t txSigningMode,
                                                 const ext_credential_t* dRepCredential);
security_policy_t policyForSignTxCertificateStakePoolRetirement(
    sign_tx_signingmode_t txSigningMode,
    const ext_credential_t* poolCredential,
    uint64_t epoch);
security_policy_t policyForSignTxStakePoolRegistrationInit(sign_tx_signingmode_t txSigningMode,
                                                           uint32_t numOwners,
                                                           uint32_t numPathOwners);
security_policy_t policyForSignTxStakePoolRegistrationPoolId(sign_tx_signingmode_t txSigningMode,
                                                             const pool_id_t* poolId);
security_policy_t policyForSignTxStakePoolRegistrationVrfKey(sign_tx_signingmode_t txSigningMode);
security_policy_t policyForSignTxStakePoolRegistrationRewardAccount(
    sign_tx_signingmode_t txSigningMode,
    const reward_account_t* poolRewardAccount);
security_policy_t policyForSignTxStakePoolRegistrationOwner(
    const sign_tx_signingmode_t txSigningMode,
    const pool_owner_t* owner);
security_policy_t policyForSignTxStakePoolRegistrationRelay(
    const sign_tx_signingmode_t txSigningMode,
    const pool_relay_t* relay);
security_policy_t policyForSignTxStakePoolRegistrationMetadata();
security_policy_t policyForSignTxStakePoolRegistrationNoMetadata();
security_policy_t policyForSignTxStakePoolRegistrationConfirm(uint32_t numOwners,
                                                              uint32_t numRelays);

security_policy_t policyForSignTxWithdrawal(sign_tx_signingmode_t txSigningMode,
                                            const ext_credential_t* stakeCredential,
                                            warning_bits_t* warnings);
security_policy_t policyForSignTxAuxData(aux_data_type_t auxDataType);

security_policy_t policyForSignTxValidityIntervalStart();
security_policy_t policyForSignTxMintInit(const sign_tx_signingmode_t txSigningMode);

security_policy_t policyForSignTxScriptDataHash(const sign_tx_signingmode_t txSigningMode);

security_policy_t policyForSignTxCollateralInput(const sign_tx_signingmode_t txSigningMode,
                                                 bool isTotalCollateralIncluded);

security_policy_t policyForSignTxRequiredSigner(const sign_tx_signingmode_t txSigningMode,
                                                required_signer_t* requiredSigner);

security_policy_t policyForSignTxWitness(sign_tx_signingmode_t txSigningMode,
                                         const bip44_path_t* witnessPath,
                                         bool mintPresent,
                                         const bip44_path_t* poolOwnerPath,
                                         warning_bits_t* warnings);
security_policy_t policyForSignTxTotalCollateral();

security_policy_t policyForSignTxReferenceInput(const sign_tx_signingmode_t txSigningMode);

security_policy_t policyForSignTxVotingProcedure(sign_tx_signingmode_t txSigningMode,
                                                 ext_voter_t* voter);

security_policy_t policyForSignTxTreasury(sign_tx_signingmode_t txSigningMode, uint64_t treasury);

security_policy_t policyForSignTxDonation(sign_tx_signingmode_t txSigningMode, uint64_t donation);

security_policy_t policyForSignTxConfirm();

security_policy_t policyForSignOpCert(const bip44_path_t* poolColdKeyPathSpec,
                                      warning_bits_t* warnings);

security_policy_t policyForCVoteRegistrationVoteKey();
security_policy_t policyForCVoteRegistrationVoteKeyPath(bip44_path_t* path,
                                                        cvote_registration_format_t format);
security_policy_t policyForCVoteRegistrationStakingKey(const bip44_path_t* stakingKeyPath);
security_policy_t policyForCVoteRegistrationPaymentDestination(
    const tx_output_destination_storage_t* destination,
    const uint8_t networkId);
security_policy_t policyForCVoteRegistrationNonce();
security_policy_t policyForCVoteRegistrationVotingPurpose();
security_policy_t policyForCVoteRegistrationConfirm();

security_policy_t policyForSignCVoteInit();
security_policy_t policyForSignCVoteConfirm();
security_policy_t policyForSignCVoteWitness(bip44_path_t* path);

security_policy_t policyForSignMsg(const bip44_path_t* witnessPath,
                                   cip8_address_field_type_t addressFieldType,
                                   const addressParams_t* addressParams);
