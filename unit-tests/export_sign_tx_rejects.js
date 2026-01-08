#!/usr/bin/env node

"use strict"

require("ts-node/register")

const path = require("path")

const {
  transactionInitRejectTestCases,
  addressParamsRejectTestCases,
  certificateRejectTestCases,
  certificateStakingRejectTestCases,
  certificateStakePoolRetirementRejectTestCases,
  withdrawalRejectTestCases,
  witnessRejectTestCases,
  singleAccountRejectTestCases,
  collateralOutputRejectTestCases,
  testsInvalidTokenBundleOrdering,
  poolRegistrationOwnerRejectTestCases,
  stakePoolRegistrationOwnerRejectTestCases,
  stakePoolRegistrationPoolIdRejectTestCases,
  outputRejectTestCases,
  invalidCertificates,
  testsCVoteRegistrationRejects,
  invalidPoolMetadataTestCases,
  invalidRelayTestCases,
} = require(
  path.resolve(__dirname, "../../ledgerjs-cardano-shelley/test/integration/__fixtures__/signTxRejects.ts"),
)

const {InvalidDataReason} = require(
  path.resolve(__dirname, "../../ledgerjs-cardano-shelley/src/errors/invalidDataReason"),
)

const invalidDataReasonLookup = new Map(
  Object.entries(InvalidDataReason).map(([key, value]) => [value, key]),
)

const normalizeRejectReason = (reason) => {
  if (!reason) {
    return reason
  }
  const entry = invalidDataReasonLookup.get(reason)
  return entry ? `InvalidDataReason.${entry}` : reason
}

const fixtures = {
  transactionInitRejectTestCases,
  addressParamsRejectTestCases,
  certificateRejectTestCases,
  certificateStakingRejectTestCases,
  certificateStakePoolRetirementRejectTestCases,
  withdrawalRejectTestCases,
  witnessRejectTestCases,
  singleAccountRejectTestCases,
  collateralOutputRejectTestCases,
  testsInvalidTokenBundleOrdering,
  poolRegistrationOwnerRejectTestCases,
  stakePoolRegistrationOwnerRejectTestCases,
  stakePoolRegistrationPoolIdRejectTestCases,
  outputRejectTestCases,
  invalidCertificates,
  testsCVoteRegistrationRejects,
  invalidPoolMetadataTestCases,
  invalidRelayTestCases,
}

const normalizedFixtures = {}
for (const [setName, entries] of Object.entries(fixtures)) {
  if (!entries) {
    continue
  }

  normalizedFixtures[setName] = entries.map((entry) => ({
    ...entry,
    rejectReason: normalizeRejectReason(entry.rejectReason),
  }))
}

process.stdout.write(JSON.stringify(normalizedFixtures))
