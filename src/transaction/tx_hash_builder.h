#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "hash.h"
#include "addressUtils/addressUtilsShelley.h"
#include "transaction/tx.h"
#include "transaction/tx_output_types.h"

enum {
    TX_BODY_KEY_INPUTS = 0,
    TX_BODY_KEY_OUTPUTS = 1,
    TX_BODY_KEY_FEE = 2,
    TX_BODY_KEY_TTL = 3,
    TX_BODY_KEY_CERTIFICATES = 4,
    TX_BODY_KEY_WITHDRAWALS = 5,
    // TX_BODY_KEY_UPDATE = 6, // not used
    TX_BODY_KEY_AUX_DATA = 7,
    TX_BODY_KEY_VALIDITY_INTERVAL_START = 8,
    TX_BODY_KEY_MINT = 9,
    TX_BODY_KEY_SCRIPT_HASH_DATA = 11,
    TX_BODY_KEY_COLLATERAL_INPUTS = 13,
    TX_BODY_KEY_REQUIRED_SIGNERS = 14,
    TX_BODY_KEY_NETWORK_ID = 15,
    TX_BODY_KEY_COLLATERAL_OUTPUT = 16,
    TX_BODY_KEY_TOTAL_COLLATERAL = 17,
    TX_BODY_KEY_REFERENCE_INPUTS = 18,
    TX_BODY_KEY_VOTING_PROCEDURES = 19,
    // TX_BODY_KEY_PROPOSAL_PROCEDURES = 20, // not used
    TX_BODY_KEY_TREASURY = 21,
    TX_BODY_KEY_DONATION = 22,
};

enum {
    TX_OUTPUT_KEY_ADDRESS = 0,
    TX_OUTPUT_KEY_VALUE = 1,
    TX_OUTPUT_KEY_DATUM_OPTION = 2,
    TX_OUTPUT_KEY_SCRIPT_REF = 3,
};

/* The state machine of the tx hash builder is driven by user calls.
 * E.g., when the user calls txHashBuilder_addInput(), the input is only
 * added and the state is not advanced to outputs even if all inputs have been added
 * --- only after calling txHashBuilder_enterOutputs()
 * is the state advanced to TX_HASH_BUILDER_IN_OUTPUTS.
 *
 * Pool registration certificates have an inner state loop which is implemented
 * in a similar fashion with the exception that when all pool certificate data
 * have been entered, the state is changed to TX_HASH_BUILDER_IN_CERTIFICATES.
 */
typedef enum {
    TX_HASH_BUILDER_INIT = 100,
    TX_HASH_BUILDER_IN_INPUTS = 200,
    TX_HASH_BUILDER_IN_OUTPUTS = 300,
    TX_HASH_BUILDER_IN_FEE = 400,
    TX_HASH_BUILDER_IN_TTL = 500,
    TX_HASH_BUILDER_IN_CERTIFICATES = 600,
    TX_HASH_BUILDER_IN_CERTIFICATES_POOL_INIT = 610,
    TX_HASH_BUILDER_IN_CERTIFICATES_POOL_KEY_HASH = 611,
    TX_HASH_BUILDER_IN_CERTIFICATES_POOL_VRF = 612,
    TX_HASH_BUILDER_IN_CERTIFICATES_POOL_FINANCIALS = 613,
    TX_HASH_BUILDER_IN_CERTIFICATES_POOL_REWARD_ACCOUNT = 614,
    TX_HASH_BUILDER_IN_CERTIFICATES_POOL_OWNERS = 615,
    TX_HASH_BUILDER_IN_CERTIFICATES_POOL_RELAYS = 616,
    TX_HASH_BUILDER_IN_CERTIFICATES_POOL_METADATA = 617,
    TX_HASH_BUILDER_IN_WITHDRAWALS = 700,
    TX_HASH_BUILDER_IN_AUX_DATA = 800,
    TX_HASH_BUILDER_IN_VALIDITY_INTERVAL_START = 900,
    TX_HASH_BUILDER_IN_MINT = 1000,
    TX_HASH_BUILDER_IN_SCRIPT_DATA_HASH = 1100,
    TX_HASH_BUILDER_IN_COLLATERAL_INPUTS = 1200,
    TX_HASH_BUILDER_IN_REQUIRED_SIGNERS = 1300,
    TX_HASH_BUILDER_IN_NETWORK_ID = 1400,
    TX_HASH_BUILDER_IN_COLLATERAL_OUTPUT = 1500,
    TX_HASH_BUILDER_IN_TOTAL_COLLATERAL = 1600,
    TX_HASH_BUILDER_IN_REFERENCE_INPUTS = 1700,
    TX_HASH_BUILDER_IN_VOTING_PROCEDURES = 1800,
    TX_HASH_BUILDER_IN_TREASURY = 1900,
    TX_HASH_BUILDER_IN_DONATION = 2000,
    TX_HASH_BUILDER_FINISHED = 2100,
} tx_hash_builder_state_t;

typedef enum {
    TX_OUTPUT_INIT = 10,            //  tx_hash_builder_state moved to TX_HASH_BUILDER_IN_OUTPUTS
    TX_OUTPUT_TOP_LEVEL_DATA = 11,  // output address was added, coin was added, multiasset map is
                                    // being added (if included)
    TX_OUTPUT_ASSET_GROUP = 13,     // asset group map is being added
    TX_OUTPUT_DATUM_HASH = 20,      //  Datum hash added
    TX_OUTPUT_DATUM_INLINE = 21,    //  Inline datum is being added in chunks
    TX_OUTPUT_SCRIPT_REFERENCE_CHUNKS = 31,  // Script reference is being added
} tx_hash_builder_output_state_t;

typedef struct {
    bool tagCborSets;

    uint16_t remainingInputs;
    uint16_t remainingOutputs;
    uint16_t remainingWithdrawals;
    uint16_t remainingCertificates;
    uint16_t remainingCollateralInputs;
    uint16_t remainingRequiredSigners;
    uint16_t remainingReferenceInputs;
    uint16_t remainingVotingProcedures;
    bool includeTtl;
    bool includeAuxData;
    bool includeValidityIntervalStart;
    bool includeMint;
    bool includeScriptDataHash;
    bool includeNetworkId;
    bool includeCollateralOutput;
    bool includeTotalCollateral;
    bool includeTreasury;
    bool includeDonation;

    union {
        struct {
            uint16_t remainingOwners;
            uint16_t remainingRelays;
        } poolCertificateData;

        struct {
            tx_hash_builder_output_state_t outputState;
            tx_output_serialization_format_t serializationFormat;
            bool includeDatum;
            bool includeRefScript;

            union {
                // this is also used for mint, but needs to coexist with output data
                // so we want don't want to move it up one level in the unions
                struct {
                    uint16_t remainingAssetGroups;
                    uint16_t remainingTokens;
                } multiassetData;

                struct {
                    size_t remainingBytes;
                } datumData;

                struct {
                    size_t remainingBytes;
                } referenceScriptData;
            };
        } outputData;
    };

    tx_hash_builder_state_t state;
    blake2b_256_context_t txHash;
} tx_hash_builder_t;

typedef struct {
    tx_output_serialization_format_t format;

    tx_output_destination_t destination;
    uint64_t amount;

    uint16_t numAssetGroups;
    bool includeDatum;
    bool includeRefScript;
} tx_output_description_t;

void txHashBuilder_init(tx_hash_builder_t* builder,
                        bool tagCborSets,
                        uint16_t numInputs,
                        uint16_t numOutputs,
                        bool includeTtl,
                        uint16_t numCertificates,
                        uint16_t numWithdrawals,
                        bool includeAuxData,
                        bool includeValidityIntervalStart,
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
                        bool includeDonation);

void txHashBuilder_enterInputs(tx_hash_builder_t* builder);

void txHashBuilder_addInput(tx_hash_builder_t* builder, const tx_input_t* input);

void txHashBuilder_enterOutputs(tx_hash_builder_t* builder);

void txHashBuilder_addOutput_topLevelData(tx_hash_builder_t* builder,
                                          const tx_output_description_t* output);

void txHashBuilder_addOutput_tokenGroup(tx_hash_builder_t* builder,
                                        const uint8_t* policyIdBuffer,
                                        size_t policyIdSize,
                                        uint16_t numTokens);

void txHashBuilder_addOutput_token(tx_hash_builder_t* builder,
                                   const uint8_t* assetNameBuffer,
                                   size_t assetNameSize,
                                   uint64_t amount);

#define MAX_CBOR_VOTER_MAP_KEY_SIZE 64
#define MAX_CBOR_GOV_ACTION_MAP_KEY_SIZE 72

void txHashBuilder_serializeVoterKey(const ext_voter_t* voter,
                                     uint8_t* buffer,
                                     size_t bufferLen,
                                     size_t* bytesWritten);

void txHashBuilder_serializeGovActionKey(const gov_action_id_t* govActionId,
                                         uint8_t* buffer,
                                         size_t bufferLen,
                                         size_t* bytesWritten);

void txHashBuilder_addOutput_datum(tx_hash_builder_t* builder,
                                   datum_type_t datumType,
                                   const uint8_t* buffer,
                                   size_t bufferSize);

void txHashBuilder_addOutput_datum_inline_chunk(tx_hash_builder_t* builder,
                                                const uint8_t* buffer,
                                                size_t bufferSize);

void txHashBuilder_addOutput_referenceScript(tx_hash_builder_t* builder, size_t bufferSize);

void txHashBuilder_addOutput_referenceScript_dataChunk(tx_hash_builder_t* builder,
                                                       const uint8_t* buffer,
                                                       size_t bufferSize);

void txHashBuilder_addFee(tx_hash_builder_t* builder, uint64_t fee);

void txHashBuilder_addTtl(tx_hash_builder_t* builder, uint64_t ttl);

void txHashBuilder_enterCertificates(tx_hash_builder_t* builder);

void txHashBuilder_addCertificate_stakingOld(tx_hash_builder_t* builder,
                                             const certificate_type_t certificateType,
                                             const ext_credential_t* stakingCredential);
void txHashBuilder_addCertificate_staking(tx_hash_builder_t* builder,
                                          const certificate_type_t certificateType,
                                          const ext_credential_t* stakeCredential,
                                          uint64_t deposit);

void txHashBuilder_addCertificate_stakeDelegation(tx_hash_builder_t* builder,
                                                  const ext_credential_t* stakeCredential,
                                                  const uint8_t* poolKeyHash,
                                                  size_t poolKeyHashSize);

void txHashBuilder_addCertificate_voteDelegation(tx_hash_builder_t* builder,
                                                 const ext_credential_t* stakeCredential,
                                                 const ext_drep_t* drep);

void txHashBuilder_addCertificate_committeeAuthHot(tx_hash_builder_t* builder,
                                                   const ext_credential_t* coldCredential,
                                                   const ext_credential_t* hotCredential);

void txHashBuilder_addCertificate_committeeResign(tx_hash_builder_t* builder,
                                                  const ext_credential_t* coldCredential,
                                                  const anchor_t* anchor);

void txHashBuilder_addCertificate_dRepRegistration(tx_hash_builder_t* builder,
                                                   const ext_credential_t* dRepCredential,
                                                   uint64_t deposit,
                                                   const anchor_t* anchor);

void txHashBuilder_addCertificate_dRepDeregistration(tx_hash_builder_t* builder,
                                                     const ext_credential_t* dRepCredential,
                                                     uint64_t deposit);

void txHashBuilder_addCertificate_dRepUpdate(tx_hash_builder_t* builder,
                                             const ext_credential_t* dRepCredential,
                                             const anchor_t* anchor);

void txHashBuilder_addCertificate_poolRetirement(tx_hash_builder_t* builder,
                                                 const uint8_t* poolKeyHash,
                                                 size_t poolKeyHashSize,
                                                 uint64_t epoch);

void txHashBuilder_poolRegistrationCertificate_enter(tx_hash_builder_t* builder,
                                                     uint16_t numOwners,
                                                     uint16_t numRelays);

void txHashBuilder_poolRegistrationCertificate_poolKeyHash(tx_hash_builder_t* builder,
                                                           const uint8_t* poolKeyHash,
                                                           size_t poolKeyHashSize);

void txHashBuilder_poolRegistrationCertificate_vrfKeyHash(tx_hash_builder_t* builder,
                                                          const uint8_t* vrfKeyHash,
                                                          size_t vrfKeyHashSize);

void txHashBuilder_poolRegistrationCertificate_financials(tx_hash_builder_t* builder,
                                                          uint64_t pledge,
                                                          uint64_t cost,
                                                          uint64_t marginNumerator,
                                                          uint64_t marginDenominator);

void txHashBuilder_poolRegistrationCertificate_rewardAccount(tx_hash_builder_t* builder,
                                                             const uint8_t* rewardAccount,
                                                             size_t rewardAccountSize);

void txHashBuilder_addPoolRegistrationCertificate_enterOwners(tx_hash_builder_t* builder);

void txHashBuilder_addPoolRegistrationCertificate_addOwner(tx_hash_builder_t* builder,
                                                           const uint8_t* stakingKeyHash,
                                                           size_t stakingKeyHashSize);

void txHashBuilder_addPoolRegistrationCertificate_enterRelays(tx_hash_builder_t* builder);

void txHashBuilder_addPoolRegistrationCertificate_addRelay(tx_hash_builder_t* builder,
                                                           const pool_relay_t* relay);

void txHashBuilder_addPoolRegistrationCertificate_addPoolMetadata(tx_hash_builder_t* builder,
                                                                  const uint8_t* url,
                                                                  size_t urlSize,
                                                                  const uint8_t* metadataHash,
                                                                  size_t metadataHashSize);

void txHashBuilder_addPoolRegistrationCertificate_addPoolMetadata_null(tx_hash_builder_t* builder);

void txHashBuilder_enterWithdrawals(tx_hash_builder_t* builder);

void txHashBuilder_addWithdrawal(tx_hash_builder_t* builder,
                                 const uint8_t* rewardAddressBuffer,
                                 size_t rewardAddressSize,
                                 uint64_t amount);

void txHashBuilder_addAuxData(tx_hash_builder_t* builder,
                              const uint8_t* auxDataHashBuffer,
                              size_t auxDataHashSize);

void txHashBuilder_addValidityIntervalStart(tx_hash_builder_t* builder,
                                            uint64_t validityIntervalStart);

void txHashBuilder_enterMint(tx_hash_builder_t* builder);

void txHashBuilder_addMint_topLevelData(tx_hash_builder_t* builder, uint16_t numAssetGroups);

void txHashBuilder_addMint_tokenGroup(tx_hash_builder_t* builder,
                                      const uint8_t* policyIdBuffer,
                                      size_t policyIdSize,
                                      uint16_t numTokens);

void txHashBuilder_addMint_token(tx_hash_builder_t* builder,
                                 const uint8_t* assetNameBuffer,
                                 size_t assetNameSize,
                                 int64_t amount);

void txHashBuilder_addScriptDataHash(tx_hash_builder_t* builder,
                                     const uint8_t* scriptHashData,
                                     size_t scriptHashDataSize);

void txHashBuilder_enterCollateralInputs(tx_hash_builder_t* builder);

void txHashBuilder_addCollateralInput(tx_hash_builder_t* builder, const tx_input_t* collInput);

void txHashBuilder_enterRequiredSigners(tx_hash_builder_t* builder);

void txHashBuilder_addRequiredSigner(tx_hash_builder_t* builder,
                                     const uint8_t* vkeyBuffer,
                                     size_t vkeySize);

void txHashBuilder_addNetworkId(tx_hash_builder_t* builder, uint8_t networkId);

void txHashBuilder_addCollateralOutput(tx_hash_builder_t* builder,
                                       const tx_output_description_t* output);

void txHashBuilder_addCollateralOutput_tokenGroup(tx_hash_builder_t* builder,
                                                  const uint8_t* policyIdBuffer,
                                                  size_t policyIdSize,
                                                  uint16_t numTokens);

void txHashBuilder_addCollateralOutput_token(tx_hash_builder_t* builder,
                                             const uint8_t* assetNameBuffer,
                                             size_t assetNameSize,
                                             uint64_t amount);

void txHashBuilder_addTotalCollateral(tx_hash_builder_t* builder, uint64_t txColl);

void txHashBuilder_enterReferenceInputs(tx_hash_builder_t* builder);

void txHashBuilder_addReferenceInput(tx_hash_builder_t* builder, const tx_input_t* refInput);

void txHashBuilder_enterVotingProcedures(tx_hash_builder_t* builder);

void txHashBuilder_addVoter(tx_hash_builder_t* builder,
                            ext_voter_t* voter,
                            uint16_t numVotes);

void txHashBuilder_addVote(tx_hash_builder_t* builder,
                           gov_action_id_t* govActionId,
                           voting_procedure_t* votingProcedure);

void txHashBuilder_addTreasury(tx_hash_builder_t* builder, uint64_t treasury);

void txHashBuilder_addDonation(tx_hash_builder_t* builder, uint64_t donation);

void txHashBuilder_finalize(tx_hash_builder_t* builder, uint8_t* outBuffer, size_t outSize);

#ifdef TRACE_TX_HASH_BUILDER
size_t txHashBuilder_get_trace_body(uint8_t* outBuffer, size_t outMaxSize);
#endif
