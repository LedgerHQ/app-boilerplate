# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Sign TX check
"""

from enum import IntEnum
from typing import List, Optional, Union
from dataclasses import dataclass, field
import base58

from application_client.app_def import Errors, NetworkDesc, Mainnet, Testnet, Testnet_legacy
from standalone.input_files.derive_address import DeriveAddressTestCase, AddressType, pointer_to_str


MAX_SIGN_TX_CHUNK_SIZE = 240

class TransactionSigningMode(IntEnum):
    ORDINARY_TRANSACTION = 0x03
    POOL_REGISTRATION_AS_OWNER = 0x04
    POOL_REGISTRATION_AS_OPERATOR = 0x05
    MULTISIG_TRANSACTION = 0x06
    PLUTUS_TRANSACTION = 0x07

class TxAuxiliaryDataType(IntEnum):
    ARBITRARY_HASH = 0x00
    CIP36_REGISTRATION = 0x01

class CredentialParamsType(IntEnum):
    KEY_PATH = 0x00
    SCRIPT_HASH = 0x01
    KEY_HASH = 0x02

class TxOutputFormat(IntEnum):
    ARRAY_LEGACY = 0x00
    MAP_BABBAGE = 0x01

class TxOutputDestinationType(IntEnum):
    THIRD_PARTY = 0x01
    DEVICE_OWNED = 0x02

class PoolKeyType(IntEnum):
    DEVICE_OWNED = 0x01
    THIRD_PARTY = 0x02

class VoteOption(IntEnum):
    NO = 0x00
    YES = 0x01
    ABSTAIN = 0x02

class VoterType(IntEnum):
    COMMITTEE_KEY_HASH = 0
    COMMITTEE_KEY_PATH = 100
    COMMITTEE_SCRIPT_HASH = 1
    DREP_KEY_HASH = 2
    DREP_KEY_PATH = 102
    DREP_SCRIPT_HASH = 3
    STAKE_POOL_KEY_HASH = 4
    STAKE_POOL_KEY_PATH = 104

class CertificateType(IntEnum):
    STAKE_REGISTRATION = 0
    STAKE_DEREGISTRATION = 1
    STAKE_DELEGATION = 2
    STAKE_POOL_REGISTRATION = 3
    STAKE_POOL_RETIREMENT = 4
    STAKE_REGISTRATION_CONWAY = 7
    STAKE_DEREGISTRATION_CONWAY = 8
    VOTE_DELEGATION = 9
    AUTHORIZE_COMMITTEE_HOT = 14
    RESIGN_COMMITTEE_COLD = 15
    DREP_REGISTRATION = 16
    DREP_DEREGISTRATION = 17
    DREP_UPDATE = 18

class CIP36VoteRegistrationFormat(IntEnum):
    CIP_15 = 1
    CIP_36 = 2

class CIP36VoteDelegationType(IntEnum):
    KEY = 1
    PATH = 2

class DRepParamsType(IntEnum):
    KEY_HASH = 0
    SCRIPT_HASH = 1
    ABSTAIN = 2
    NO_CONFIDENCE = 3
    KEY_PATH = 100

class TxRequiredSignerType(IntEnum):
    PATH = 0
    HASH = 1

class DatumType(IntEnum):
    HASH = 0
    INLINE = 1

class RelayType(IntEnum):
    SINGLE_HOST_IP_ADDR = 0
    SINGLE_HOST_HOSTNAME = 1
    MULTI_HOST = 2


@dataclass
class TxInput:
    txHashHex: str
    path: Optional[str] = None
    outputIndex: int = 0


@dataclass
class Token:
    assetNameHex: str
    amount: int


@dataclass
class AssetGroup:
    policyIdHex: str
    tokens: List[Token]


@dataclass
class ThirdPartyAddressParams:
    addressHex: str


@dataclass
class TxOutputDestination:
    type: TxOutputDestinationType
    params: Union[ThirdPartyAddressParams, DeriveAddressTestCase]


@dataclass
class Datum:
    type: DatumType
    datumHex: str


@dataclass
class TxOutputAlonzo:
    destination: TxOutputDestination
    amount: int
    format: TxOutputFormat = TxOutputFormat.ARRAY_LEGACY
    tokenBundle: List[AssetGroup] = field(default_factory=list)
    datum: Optional[Datum] = None


@dataclass
class TxOutputBabbage:
    destination: TxOutputDestination
    amount: int
    format: TxOutputFormat = TxOutputFormat.MAP_BABBAGE
    tokenBundle: List[AssetGroup] = field(default_factory=list)
    datum: Optional[Datum] = None
    referenceScriptHex: Optional[str] = None

TxOutput = Union[TxOutputAlonzo, TxOutputBabbage]


@dataclass
class TxAuxiliaryDataHash:
    hashHex: str


@dataclass
class CIP36VoteDelegation:
    type: CIP36VoteDelegationType
    votingKeyPath: str
    weight: int


@dataclass
class TxAuxiliaryDataCIP36:
    format: CIP36VoteRegistrationFormat
    stakingPath: str
    paymentDestination: TxOutputDestination
    nonce: int
    voteKey: Optional[str] = None
    votingPurpose: Optional[int] = None
    delegations: List[CIP36VoteDelegation] = field(default_factory=list)


@dataclass
class TxAuxiliaryData:
    type: TxAuxiliaryDataType
    params: Union[TxAuxiliaryDataHash, TxAuxiliaryDataCIP36]


@dataclass
class RequiredSigner:
    type: TxRequiredSignerType
    addressHex: str  # signerPath or signerHash


@dataclass
class CredentialParams:
    type: CredentialParamsType
    keyValue: Optional[str] = None  # keyPath, keyHash or scriptHash


@dataclass
class Withdrawal:
    stakeCredential: CredentialParams
    amount: int


@dataclass
class DRepParams:
    type: DRepParamsType
    keyValue: Optional[str] = None  # keyPath, keyHash or scriptHash


@dataclass
class GovActionId:
    txHashHex: str
    govActionIndex: int


@dataclass
class AnchorParams:
    url: str
    hashHex: str


@dataclass
class VotingProcedure:
    vote: VoteOption
    anchor: Optional[AnchorParams] = None


@dataclass
class Voter:
    type: VoterType
    keyValue: str  # keyPath, keyHash or scriptHash


@dataclass
class Vote:
    govActionId: GovActionId
    votingProcedure: VotingProcedure


@dataclass
class VoterVotes:
    voter: Voter
    votes: List[Vote]


@dataclass
class StakeRegistrationParams:
    stakeCredential: CredentialParams

@dataclass
class StakeRegistrationConwayParams:
    stakeCredential: CredentialParams
    deposit: int

@dataclass
class StakeDelegationParams:
    stakeCredential: CredentialParams
    poolKeyHash: str

@dataclass
class VoteDelegationParams:
    stakeCredential: CredentialParams
    dRep: DRepParams

@dataclass
class AuthorizeCommitteeParams:
    coldCredential: CredentialParams
    hotCredential: CredentialParams

@dataclass
class ResignCommitteeParams:
    coldCredential: CredentialParams
    anchor: Optional[AnchorParams] = None

@dataclass
class DRepRegistrationParams:
    dRepCredential: CredentialParams
    deposit: int
    anchor: Optional[AnchorParams] = None

@dataclass
class DRepUpdateParams:
    dRepCredential: CredentialParams
    anchor: Optional[AnchorParams] = None

@dataclass
class PoolRetirementParams:
    poolKeyPath: str
    retirementEpoch: int

@dataclass
class Margin:
    numerator: int
    denominator: int

@dataclass
class PoolMetadataParams:
    metadataUrl: str
    metadataHashHex: str

@dataclass
class PoolKey:  # same for PoolRewardAccount and PoolOwner
    type: PoolKeyType
    key: str  # hex string or path

@dataclass
class SingleHostIpAddrRelayParams:
    portNumber: Optional[int] = None
    ipv4: Optional[str] = None
    ipv6: Optional[str] = None

@dataclass
class SingleHostHostnameRelayParams:
    portNumber: int
    dnsName: str

@dataclass
class MultiHostRelayParams:
    dnsName: str

@dataclass
class Relay:
    type: RelayType
    params: Union[SingleHostIpAddrRelayParams, SingleHostHostnameRelayParams, MultiHostRelayParams]

@dataclass
class PoolRegistrationParams:
    poolKey: PoolKey
    vrfKeyHashHex: str
    pledge: int
    cost: int
    margin: Margin
    rewardAccount: PoolKey
    poolOwners: List[PoolKey]
    relays: List[Relay]
    metadata: Optional[PoolMetadataParams] = None

@dataclass
class Certificate:
    type: CertificateType
    params: Union[StakeRegistrationParams,
                  StakeRegistrationConwayParams,
                  StakeDelegationParams,
                  VoteDelegationParams,
                  AuthorizeCommitteeParams,
                  ResignCommitteeParams,
                  DRepRegistrationParams,
                  DRepUpdateParams,
                  PoolRegistrationParams,
                  PoolRetirementParams]

@dataclass
class Transaction:
    network: NetworkDesc
    inputs: List[TxInput]
    outputs: List[TxOutput]
    fee: int
    ttl: Optional[int] = None
    certificates: List[Certificate] = field(default_factory=list)
    withdrawals: List[Withdrawal] = field(default_factory=list)
    mint: List[AssetGroup] = field(default_factory=list)
    collateralInputs: List[TxInput] = field(default_factory=list)
    requiredSigners: List[RequiredSigner] = field(default_factory=list)
    referenceInputs: List[TxInput] = field(default_factory=list)
    votingProcedures: List[VoterVotes] = field(default_factory=list)
    auxiliaryData: Optional[TxAuxiliaryData] = None
    validityIntervalStart: Optional[int] = None
    scriptDataHash: Optional[str] = None
    includeNetworkId: Optional[bool] = None
    collateralOutput: Optional[TxOutput] = None
    totalCollateral: Optional[int] = None
    treasury: Optional[int] = None
    donation: Optional[int] = None


@dataclass
class Witness:
    path: str
    witnessSignatureHex: Optional[str] = None


@dataclass
class SignedTransactionData:
    witnesses: Optional[List[Witness]] = None
    sw: Optional[Errors] = Errors.SW_SUCCESS


@dataclass
class SignTxTestCase:
    name: str
    tx: Transaction
    signingMode: TransactionSigningMode
    txBody: str
    expected: SignedTransactionData
    options: bool = False
    additionalWitnessPaths: List[str] = field(default_factory=list)
    # TODO: Debug navigation
    nano_skip: Optional[bool] = False


# pylint: disable=line-too-long
inputs: dict[str, TxInput] = {
    "utxoByron": TxInput("1af8fa0b754ff99253d983894e63a2b09cbb56c833ba18c3384210163f63dcfc", "m/44'/1815'/0'/0/0"),
    "utxoShelley": TxInput("3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7", "m/1852'/1815'/0'/0/0"),
    "utxoShelley2": TxInput("3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7", "m/1852'/1815'/0'/2/1"),
    "utxoNonReasonable": TxInput("3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7", "m/1852'/1815'/456'/0/0"),
    "utxoMultisig": TxInput("3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7"),
    "utxoNoPath": TxInput("3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7"),
    "utxoWithPath0": TxInput("3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7", "m/1852'/1815'/0'/0/0"),
}

destinations: dict[str, TxOutputDestination] = {
    "externalByronMainnet":
        TxOutputDestination(TxOutputDestinationType.THIRD_PARTY,
                            ThirdPartyAddressParams(base58.b58decode("Ae2tdPwUPEZCanmBz5g2GEwFqKTKpNJcGYPKfDxoNeKZ8bRHr8366kseiK2").hex())),
    "externalByronDaedalusMainnet":
        TxOutputDestination(TxOutputDestinationType.THIRD_PARTY,
                            ThirdPartyAddressParams(base58.b58decode("DdzFFzCqrht7HGoJ87gznLktJGywK1LbAJT2sbd4txmgS7FcYLMQFhawb18ojS9Hx55mrbsHPr7PTraKh14TSQbGBPJHbDZ9QVh6Z6Di").hex())),
    "externalByronTestnet":
        TxOutputDestination(TxOutputDestinationType.THIRD_PARTY,
                            ThirdPartyAddressParams(base58.b58decode("2657WMsDfac6Cmfg4Varph2qyLKGi2K9E8jrtvjHVzfSjmbTMGy5sY3HpxCKsmtDA").hex())),
    "internalBaseWithStakingPath":
        TxOutputDestination(TxOutputDestinationType.DEVICE_OWNED,
                            DeriveAddressTestCase("",
                                                  Mainnet,
                                                  AddressType.BASE_PAYMENT_KEY_STAKE_KEY,
                                                  "m/1852'/1815'/0'/0/0",
                                                  "m/1852'/1815'/0'/2/0")),
    "internalBaseWithStakingKeyHash":
        TxOutputDestination(TxOutputDestinationType.DEVICE_OWNED,
                            DeriveAddressTestCase("",
                                                  Mainnet,
                                                  AddressType.BASE_PAYMENT_KEY_STAKE_KEY,
                                                  "m/1852'/1815'/0'/0/0",
                                                  "122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277")),
    "internalEnterprise":
        TxOutputDestination(TxOutputDestinationType.DEVICE_OWNED,
                            DeriveAddressTestCase("",
                                                  Mainnet,
                                                  AddressType.ENTERPRISE_KEY,
                                                  "m/1852'/1815'/0'/0/0")),
    "internalPointer":
        TxOutputDestination(TxOutputDestinationType.DEVICE_OWNED,
                            DeriveAddressTestCase("",
                                                  Mainnet,
                                                  AddressType.POINTER_KEY,
                                                  "m/1852'/1815'/0'/0/0",
                                                  pointer_to_str(1, 2, 3))),
    "internalBaseWithStakingPathNonReasonable":
        TxOutputDestination(TxOutputDestinationType.DEVICE_OWNED,
                            DeriveAddressTestCase("",
                                                  Mainnet,
                                                  AddressType.BASE_PAYMENT_KEY_STAKE_KEY,
                                                  "m/1852'/1815'/456'/0/5000000",
                                                  "m/1852'/1815'/456'/2/0")),
    "internalBaseWithStakingPathMap":
        TxOutputDestination(TxOutputDestinationType.DEVICE_OWNED,
                            DeriveAddressTestCase("",
                                                  Mainnet,
                                                  AddressType.BASE_PAYMENT_KEY_STAKE_KEY,
                                                  "m/1852'/1815'/0'/0/0",
                                                  "m/1852'/1815'/0'/2/0")),
    "externalShelleyBaseKeyhashKeyhash":
        TxOutputDestination(TxOutputDestinationType.THIRD_PARTY,
                            # bech32 addr1q97tqh7wzy8mnx0sr2a57c4ug40zzl222877jz06nt49g4zr43fuq3k0dfpqjh3uvqcsl2qzwuwsvuhclck3scgn3vys6wkj5d
                            ThirdPartyAddressParams("017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b09")),
    "externalShelleyBaseScripthashKeyhash":
        TxOutputDestination(TxOutputDestinationType.THIRD_PARTY,
                            # bech32 addr_test1zp0z7zqwhya6mpk5q929ur897g3pp9kkgalpreny8y304rfw6j2jxnwq6enuzvt0lp89wgcsufj7mvcnxpzgkd4hz70qe8ugl4
                            ThirdPartyAddressParams("105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e")),
    "multiassetThirdParty":
        TxOutputDestination(TxOutputDestinationType.THIRD_PARTY,
                            # bech32 addr1q84sh2j72ux0l03fxndjnhctdg7hcppsaejafsa84vh7lwgmcs5wgus8qt4atk45lvt4xfxpjtwfhdmvchdf2m3u3hlsd5tq5r
                            ThirdPartyAddressParams("01eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff")),
    "trezorParityDatumHash":
        TxOutputDestination(TxOutputDestinationType.THIRD_PARTY,
                            # bech32 addr1w9rhu54nz94k9l5v6d9rzfs47h7dv7xffcwkekuxcx3evnqpvuxu0
                            ThirdPartyAddressParams("71477e52b3116b62fe8cd34a312615f5fcd678c94e1d6cdb86c1a3964c")),
    "externalShelleyBaseKeyhashScripthash":
        TxOutputDestination(TxOutputDestinationType.THIRD_PARTY,
                            # bech32 addr1yyfatq352yhh7ctw7c3s33qpwrq3pvhcmqg0yvzq9308g9msqj6hs5cg8q8zmtpf2hfrfds25jmcvpta6k5nnpzrn5eqy6fknd
                            ThirdPartyAddressParams("2113d58234512f7f616ef62308c40170c110b2f8d810f230402c5e74177004b5785308380e2dac2955d234b60aa4b786057dd5a93984439d32")),
    "paymentScriptPath":
        TxOutputDestination(TxOutputDestinationType.DEVICE_OWNED,
                            DeriveAddressTestCase("",
                                                  Mainnet,
                                                  AddressType.REWARD_KEY,
                                                  "",
                                                  "m/1852'/1815'/0'/2/0")),
    "paymentScriptHash":
        TxOutputDestination(TxOutputDestinationType.DEVICE_OWNED,
                            DeriveAddressTestCase("",
                                                  Mainnet,
                                                  AddressType.REWARD_SCRIPT,
                                                  "",
                                                  "122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277")),
    "paymentKeyPath":
        TxOutputDestination(TxOutputDestinationType.DEVICE_OWNED,
                            DeriveAddressTestCase("",
                                                  Mainnet,
                                                  AddressType.REWARD_KEY,
                                                  "",
                                                  "m/1852'/1815'/0'/2/0")),
}

outputs: dict[str, TxOutput] = {
    "externalByronMainnet": TxOutputAlonzo(destinations["externalByronMainnet"], 3003112),
    "externalByronDaedalusMainnet": TxOutputAlonzo(destinations["externalByronDaedalusMainnet"], 3003112),
    "externalByronTestnet": TxOutputAlonzo(destinations["externalByronTestnet"] , 3003112),
    "internalBaseWithStakingPath": TxOutputAlonzo(destinations["internalBaseWithStakingPath"], 7120787),
    "internalBaseWithStakingPathBabbage": TxOutputBabbage(destinations["internalBaseWithStakingPath"], 7120787),
    "internalBaseWithStakingKeyHash": TxOutputAlonzo(destinations["internalBaseWithStakingKeyHash"], 7120787),
    "internalEnterprise": TxOutputAlonzo(destinations["internalEnterprise"], 7120787),
    "internalPointer": TxOutputAlonzo(destinations["internalPointer"], 7120787),
    "internalBaseWithStakingPathNonReasonable": TxOutputAlonzo(destinations["internalBaseWithStakingPathNonReasonable"], 7120787),
    "internalBaseWithStakingPathMap": TxOutputBabbage(destinations["internalBaseWithStakingPathMap"], 7120787),
    "externalShelleyBaseKeyhashKeyhash": TxOutputAlonzo(destinations["externalShelleyBaseKeyhashKeyhash"], 1),
    "externalShelleyBaseScripthashKeyhash": TxOutputAlonzo(destinations["externalShelleyBaseScripthashKeyhash"], 1),
    "multiassetOneToken": TxOutputAlonzo(destinations["multiassetThirdParty"],
                                         1234,
                                         tokenBundle=[AssetGroup("95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("74652474436f696e", 7878754)])]),
    "multiassetManyTokens": TxOutputAlonzo(destinations["multiassetThirdParty"],
                                         1234,
                                         # fingerprints taken from CIP 14 draft
                                         tokenBundle=[AssetGroup("7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373",
                                                                 # fingerprint: asset1rjklcrnsdzqp65wjgrg55sy9723kw09mlgvlc3
                                                                 [Token("", 3),
                                                                  # fingerprint: asset17jd78wukhtrnmjh3fngzasxm8rck0l2r4hhyyt
                                                                  Token("1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df209", 1),
                                                                  # fingerprint: asset1pkpwyknlvul7az0xx8czhl60pyel45rpje4z8w
                                                                  Token("0000000000000000000000000000000000000000000000000000000000000000", 2)]),
                                                      AssetGroup("95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("456c204e69c3b16f", 1234),
                                                                  Token("74652474436f696e", 7878754)])]),
    "multiassetManyTokensBabbage": TxOutputBabbage(destinations["multiassetThirdParty"],
                                         1234,
                                         # fingerprints taken from CIP 14 draft
                                         tokenBundle=[AssetGroup("7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373",
                                                                 # fingerprint: asset1rjklcrnsdzqp65wjgrg55sy9723kw09mlgvlc3
                                                                 [Token("", 3),
                                                                  # fingerprint: asset17jd78wukhtrnmjh3fngzasxm8rck0l2r4hhyyt
                                                                  Token("1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df209", 1),
                                                                  # fingerprint: asset1pkpwyknlvul7az0xx8czhl60pyel45rpje4z8w
                                                                  Token("0000000000000000000000000000000000000000000000000000000000000000", 2)]),
                                                      AssetGroup("95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("456c204e69c3b16f", 1234),
                                                                  Token("74652474436f696e", 7878754)])]),
    "multiassetBigNumber": TxOutputAlonzo(destinations["multiassetThirdParty"],
                                         24103998870869519,
                                         tokenBundle=[AssetGroup("95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("74652474436f696e", 24103998870869519)])]),
    "multiassetChange": TxOutputAlonzo(destinations["internalBaseWithStakingPath"],
                                         1234,
                                         tokenBundle=[AssetGroup("95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("74652474436f696e", 7878754)])]),
    "multiassetDecimalPlaces": TxOutputAlonzo(destinations["multiassetThirdParty"],
                                         1234,
                                         # fingerprint: asset155nxgqj5acff7fdhc8ranfwyl7nq4ljrks7l6w
                                         tokenBundle=[AssetGroup("6954264b15bc92d6d592febeac84f14645e1ed46ca5ebb9acdb5c15f",
                                                                 [Token("5354524950", 3456789)]),
                                                      AssetGroup("af2e27f580f7f08e93190a81f72462f153026d06450924726645891b",
                                                                 # fingerprint: asset14yqf3pclzx88jjahydyfad8pxw5xhuca6j7k2p
                                                                 [Token("44524950", 1234),
                                                                  # fingerprint: asset12wejgxu04lpg6h3pm056qd207k2sfh7yjklclf
                                                                  Token("ffffffffffffffffffffffff", 1234)])]),
    "trezorParity1": TxOutputAlonzo(destinations["multiassetThirdParty"],
                                         2000000,
                                         tokenBundle=[AssetGroup("0d63e8d2c5a00cbcffbdf9112487c443466e1ea7d8c834df5ac5c425",
                                                                 [Token("74657374436f696e", 7878754)])]),
    "trezorParity2": TxOutputAlonzo(destinations["externalShelleyBaseKeyhashKeyhash"],
                                         2000000,
                                         tokenBundle=[AssetGroup("0d63e8d2c5a00cbcffbdf9112487c443466e1ea7d8c834df5ac5c425",
                                                                 [Token("74657374436f696e", 7878754)])]),
    "trezorParityDatumHash1": TxOutputAlonzo(destinations["trezorParityDatumHash"],
                                         1,
                                         datum=Datum(DatumType.HASH, "3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7")),
    "trezorParityDatumHash2": TxOutputAlonzo(destinations["trezorParityDatumHash"],
                                         1,
                                         datum=Datum(DatumType.HASH, "3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7")),
    "trezorParityBabbageOutputs": TxOutputBabbage(destinations["trezorParityDatumHash"],
                                         1,
                                         datum=Datum(DatumType.INLINE, "5579657420616e6f746865722063686f636f6c617465"),
                                         referenceScriptHex="0080f9e2c88e6c817008f3a812ed889b4a4da8e0bd103f86e7335422aa122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277"),
    "datumHashExternal": TxOutputAlonzo(destinations["externalShelleyBaseScripthashKeyhash"],
                                         7120787,
                                         datum=Datum(DatumType.HASH, "ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce188")),
    "datumHashWithTokens": TxOutputAlonzo(destinations["externalShelleyBaseScripthashKeyhash"],
                                         7120787,
                                         datum=Datum(DatumType.HASH, "ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce188"),
                                         tokenBundle=[AssetGroup("75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("7564247542686911", 47),
                                                                  Token("7564247542686912", 7878754)])]),
    "datumHashWithTokensMap": TxOutputBabbage(destinations["externalShelleyBaseScripthashKeyhash"],
                                         7120787,
                                         datum=Datum(DatumType.HASH, "ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce188"),
                                         tokenBundle=[AssetGroup("75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("7564247542686911", 47),
                                                                  Token("7564247542686912", 7878754)])]),
    "missingDatumHashWithTokens": TxOutputAlonzo(destinations["externalShelleyBaseScripthashKeyhash"],
                                         7120787,
                                         tokenBundle=[AssetGroup("75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("7564247542686911", 47),
                                                                  Token("7564247542686912", 7878754)])]),
    "inlineDatumWithTokensMap": TxOutputBabbage(destinations["externalShelleyBaseScripthashKeyhash"],
                                         7120787,
                                         datum=Datum(DatumType.INLINE, "5579657420616e6f746865722063686f636f6c617465"),
                                         tokenBundle=[AssetGroup("75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("7564247542686911", 47),
                                                                  Token("7564247542686912", 7878754)])]),
    "inlineDatum480Map": TxOutputBabbage(destinations["externalShelleyBaseScripthashKeyhash"],
                                         7120787,
                                         datum=Datum(DatumType.INLINE, "12b8240c5470b47c159597b6f71d78c7fc99d1d8d911cb19b8f50211938ef361a22d30cd8f6354ec50e99a7d3cf3e06797ed4af3d358e01b2a957caa4010da328720b9fbe7a3a6d10209a13d2eb11933eb1bf2ab02713117e421b6dcc66297c41b95ad32d3457a0e6b44d8482385f311465964c3daff226acfb7bbda47011f1a6531db30e5b5977143c48f8b8eb739487f87dc13896f58529cfb48e415fc6123e708cdc3cb15cc1900ecf88c5fc9ff66d8ad6dae18c79e4a3c392a0df4d16ffa3e370f4dad8d8e9d171c5656bb317c78a2711057e7ae0beb1dc66ba01aa69d0c0db244e6742d7758ce8da00dfed6225d4aed4b01c42a0352688ed5803f3fd64873f11355305d9db309f4a2a6673cc408a06b8827a5edef7b0fd8742627fb8aa102a084b7db72fcb5c3d1bf437e2a936b738902a9c0258b462b9f2e9befd2c6bcfc036143bb34342b9124888a5b29fa5d60909c81319f034c11542b05ca3ff6c64c7642ff1e2b25fb60dc9bb6f5c914dd4149f31896955d4d204d822deddc46f852115a479edf7521cdf4ce596805875011855158fd303c33a2a7916a9cb7acaaf5aeca7e6efb75960e9597cd845bd9a93610bf1ab47ab0de943e8a96e26a24c4996f7b07fad437829fee5bc3496192608d4c04ac642cdec7bdbb8a948ad1d434")),
    "inlineDatum304WithTokensMap": TxOutputBabbage(destinations["externalShelleyBaseScripthashKeyhash"],
                                         7120787,
                                         datum=Datum(DatumType.INLINE, "5579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f74686572206374686572"),
                                         tokenBundle=[AssetGroup("75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("7564247542686911", 47),
                                                                  Token("7564247542686912", 7878754)])]),
    "datumHashRefScriptExternalMap": TxOutputBabbage(destinations["externalShelleyBaseScripthashKeyhash"],
                                         7120787,
                                         datum=Datum(DatumType.HASH, "ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce188"),
                                         referenceScriptHex="deadbeefdeadbeefdeadbeefdeadbeefdeadbeef"),
    "datumHashRefScript240ExternalMap": TxOutputBabbage(destinations["externalShelleyBaseScripthashKeyhash"],
                                         7120787,
                                         datum=Datum(DatumType.HASH, "ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce188"),
                                         referenceScriptHex="4784392787cc567ac21d7b5346a4a89ae112b7ff7610e402284042aa4e6efca7956a53c3f5cb3ec6745f5e21150f2a77bd71a2adc3f8b9539e9bab41934b477f60a8b302584d1a619ed9b178b5ce6fcad31adc0d6fc17023ede474c09f29fdbfb290a5b30b5240fae5de71168036201772c0d272ae90220181f9bf8c3198e79fc2ae32b076abf4d0e10d3166923ce56994b25c00909e3faab8ef1358c136cd3b197488efc883a7c6cfa3ac63ca9cebc62121c6e22f594420c2abd54e78282adec20ee7dba0e6de65554adb8ee8314f23f86cf7cf0906d4b6c643966baf6c54240c19f4131374e298f38a626a4ad63e61"),
    "datumHashRefScript304ExternalMap": TxOutputBabbage(destinations["externalShelleyBaseScripthashKeyhash"],
                                         7120787,
                                         datum=Datum(DatumType.HASH, "ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce188"),
                                         referenceScriptHex="deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeaddeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeaddeadbeef"),
    "internalBaseWithTokensMap": TxOutputBabbage(destinations["internalBaseWithStakingPath"],
                                         7120787,
                                         tokenBundle=[AssetGroup("75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("7564247542686911", 47)])]),
    "multiassetInvalidAssetGroupOrdering": TxOutputBabbage(destinations["multiassetThirdParty"],
                                         1234,
                                         tokenBundle=[AssetGroup("75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("7564247542686911", 47)]),
                                                      AssetGroup("71a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39",
                                                                 [Token("7564247542686911", 47)])]),
}

mints: dict[str, List[AssetGroup]] = {
    "mintWithDecimalPlaces": [AssetGroup("6954264b15bc92d6d592febeac84f14645e1ed46ca5ebb9acdb5c15f",
                                    # fingerprint: asset155nxgqj5acff7fdhc8ranfwyl7nq4ljrks7l6w
                                    [Token("5354524950", -3456789)]),
                              AssetGroup("af2e27f580f7f08e93190a81f72462f153026d06450924726645891b",
                                    # fingerprint: asset14yqf3pclzx88jjahydyfad8pxw5xhuca6j7k2p
                                    [Token("44524950", 1234),
                                    # fingerprint: asset12wejgxu04lpg6h3pm056qd207k2sfh7yjklclf
                                     Token("ffffffffffffffffffffffff", 1234)])],
    # fingerprints taken from CIP 14 draft
    "mintAmountVariety": [AssetGroup("7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373",
                                    # fingerprint: asset1rjklcrnsdzqp65wjgrg55sy9723kw09mlgvlc3
                                    [Token("", 0),
                                     # fingerprint: asset17jd78wukhtrnmjh3fngzasxm8rck0l2r4hhyyt
                                     Token("1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df209", -1),
                                     # fingerprint: asset17jd78wukhtrnmjh3fngzasxm8rck0l2r4hhyyt (and incremented)
                                     Token("1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df20a", 9223372036854775807),
                                     # fingerprint: asset17jd78wukhtrnmjh3fngzasxm8rck0l2r4hhyyt (and incremented)
                                     Token("1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df20b", -9223372036854775808)])],
    "trezorComparison": [AssetGroup("0d63e8d2c5a00cbcffbdf9112487c443466e1ea7d8c834df5ac5c425",
                                    [Token("74657374436f696e", 7878754),
                                     Token("75657374436f696e", -7878754)])],
}

poolKeys: dict[str, PoolKey] = {
    "poolKeyPath": PoolKey(PoolKeyType.DEVICE_OWNED, "m/1853'/1815'/0'/0'"),
    "poolKeyHash": PoolKey(PoolKeyType.THIRD_PARTY, "13381d918ec0283ceeff60f7f4fc21e1540e053ccf8a77307a7a32ad"),
    "poolRewardAccountPath": PoolKey(PoolKeyType.DEVICE_OWNED, "m/1852'/1815'/3'/2/0"),
    "poolRewardAccountHash": PoolKey(PoolKeyType.THIRD_PARTY, "e1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad"),
    "stakingPathOwner0": PoolKey(PoolKeyType.DEVICE_OWNED, "m/1852'/1815'/0'/2/0"),
    "stakingPathOwner1": PoolKey(PoolKeyType.DEVICE_OWNED, "m/1852'/1815'/0'/2/1"),
    "stakingHashOwner0": PoolKey(PoolKeyType.THIRD_PARTY, "794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad"),
    "stakingHashOwner1": PoolKey(PoolKeyType.THIRD_PARTY, "0bd5d796f5e54866a14300ec2a18d706f7461b8f0502cc2a182bc88d"),
    "twoCombinedOwners": PoolKey(PoolKeyType.DEVICE_OWNED, "m/1852'/1815'/0'/2/0"),
}

relays: dict[str, Relay] = {
    "singleHostIPV4Relay0": Relay(RelayType.SINGLE_HOST_IP_ADDR,
                                  SingleHostIpAddrRelayParams(3000,
                                                              "54.228.75.154")),
    "singleHostIPV4Relay1": Relay(RelayType.SINGLE_HOST_IP_ADDR,
                                  SingleHostIpAddrRelayParams(4000,
                                                              "54.228.75.154")),
    "singleHostIPV6Relay": Relay(RelayType.SINGLE_HOST_IP_ADDR,
                                  SingleHostIpAddrRelayParams(3000,
                                                              "54.228.75.155",
                                                              "24ff:7801:33a2:e383:a5c4:340a:07c2:76e5")),
    "singleHostNameRelay": Relay(RelayType.SINGLE_HOST_HOSTNAME,
                                  SingleHostHostnameRelayParams(3000, "aaaa.bbbb.com")),
    "multiHostNameRelay": Relay(RelayType.MULTI_HOST,
                                  MultiHostRelayParams("aaaa.bbbc.com")),
}

certificates: dict[str, Certificate] = {
    "poolRegistrationDefault": Certificate(CertificateType.STAKE_POOL_REGISTRATION,
                                                        PoolRegistrationParams(poolKeys["poolKeyHash"],
                                                                               "07821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d084450",
                                                                               50000000000,
                                                                               340000000,
                                                                               Margin(3, 100),
                                                                               poolKeys["poolRewardAccountHash"],
                                                                               [poolKeys["stakingPathOwner0"]],
                                                                               [relays["singleHostIPV4Relay0"]],
                                                                               PoolMetadataParams("https://www.vacuumlabs.com/sampleUrl.json",
                                                                                                  "cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb"))),
    "poolRegistrationMixedOwners": Certificate(CertificateType.STAKE_POOL_REGISTRATION,
                                                        PoolRegistrationParams(poolKeys["poolKeyHash"],
                                                                               "07821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d084450",
                                                                               50000000000,
                                                                               340000000,
                                                                               Margin(3, 100),
                                                                               poolKeys["poolRewardAccountHash"],
                                                                               [poolKeys["stakingPathOwner0"],
                                                                                poolKeys["stakingHashOwner0"]],
                                                                               [relays["singleHostIPV4Relay0"]],
                                                                               PoolMetadataParams("https://www.vacuumlabs.com/sampleUrl.json",
                                                                                                  "cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb"))),
    "poolRegistrationMixedOwnersAllRelays": Certificate(CertificateType.STAKE_POOL_REGISTRATION,
                                                        PoolRegistrationParams(poolKeys["poolKeyHash"],
                                                                               "07821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d084450",
                                                                               50000000000,
                                                                               340000000,
                                                                               Margin(3, 100),
                                                                               poolKeys["poolRewardAccountHash"],
                                                                               [poolKeys["stakingPathOwner0"],
                                                                                poolKeys["stakingHashOwner0"]],
                                                                               [relays["singleHostIPV4Relay0"],
                                                                                relays["singleHostIPV6Relay"],
                                                                                relays["singleHostNameRelay"],
                                                                                relays["multiHostNameRelay"]],
                                                                               PoolMetadataParams("https://www.vacuumlabs.com/sampleUrl.json",
                                                                                                  "cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb"))),
    "poolRegistrationMixedOwnersIpv4SingleHostRelays": Certificate(CertificateType.STAKE_POOL_REGISTRATION,
                                                        PoolRegistrationParams(poolKeys["poolKeyHash"],
                                                                               "07821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d084450",
                                                                               50000000000,
                                                                               340000000,
                                                                               Margin(3, 100),
                                                                               poolKeys["poolRewardAccountHash"],
                                                                               [poolKeys["stakingPathOwner0"],
                                                                                poolKeys["stakingHashOwner0"]],
                                                                               [relays["singleHostIPV4Relay0"],
                                                                                relays["singleHostNameRelay"]],
                                                                               PoolMetadataParams("https://www.vacuumlabs.com/sampleUrl.json",
                                                                                                  "cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb"))),
    "poolRegistrationMixedOwnersIpv4Ipv6Relays": Certificate(CertificateType.STAKE_POOL_REGISTRATION,
                                                        PoolRegistrationParams(poolKeys["poolKeyHash"],
                                                                               "07821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d084450",
                                                                               50000000000,
                                                                               340000000,
                                                                               Margin(3, 100),
                                                                               poolKeys["poolRewardAccountHash"],
                                                                               [poolKeys["stakingPathOwner0"],
                                                                                poolKeys["stakingHashOwner0"]],
                                                                               [relays["singleHostIPV4Relay1"],
                                                                                relays["singleHostIPV6Relay"]],
                                                                               PoolMetadataParams("https://www.vacuumlabs.com/sampleUrl.json",
                                                                                                  "cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb"))),
    "poolRegistrationNoRelays": Certificate(CertificateType.STAKE_POOL_REGISTRATION,
                                                        PoolRegistrationParams(poolKeys["poolKeyHash"],
                                                                               "07821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d084450",
                                                                               50000000000,
                                                                               340000000,
                                                                               Margin(3, 100),
                                                                               poolKeys["poolRewardAccountHash"],
                                                                               [poolKeys["stakingPathOwner0"]],
                                                                               [],
                                                                               PoolMetadataParams("https://www.vacuumlabs.com/sampleUrl.json",
                                                                                                  "cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb"))),
    "poolRegistrationNoMetadata": Certificate(CertificateType.STAKE_POOL_REGISTRATION,
                                                        PoolRegistrationParams(poolKeys["poolKeyHash"],
                                                                               "07821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d084450",
                                                                               50000000000,
                                                                               340000000,
                                                                               Margin(3, 100),
                                                                               poolKeys["poolRewardAccountHash"],
                                                                               [poolKeys["stakingPathOwner0"]],
                                                                               [relays["singleHostIPV4Relay0"]])),
    "poolRegistrationOperatorNoOwnersNoRelays": Certificate(CertificateType.STAKE_POOL_REGISTRATION,
                                                        PoolRegistrationParams(poolKeys["poolKeyPath"],
                                                                               "07821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d084450",
                                                                               50000000000,
                                                                               340000000,
                                                                               Margin(3, 100),
                                                                               poolKeys["poolRewardAccountHash"],
                                                                               [],
                                                                               [],
                                                                               PoolMetadataParams("https://www.vacuumlabs.com/sampleUrl.json",
                                                                                                  "cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb"))),
    "poolRegistrationOperatorOneOwnerOperatorNoRelays": Certificate(CertificateType.STAKE_POOL_REGISTRATION,
                                                        PoolRegistrationParams(poolKeys["poolKeyPath"],
                                                                               "07821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d084450",
                                                                               50000000000,
                                                                               340000000,
                                                                               Margin(3, 100),
                                                                               poolKeys["poolRewardAccountHash"],
                                                                               [poolKeys["stakingHashOwner0"]],
                                                                               [],
                                                                               PoolMetadataParams("https://www.vacuumlabs.com/sampleUrl.json",
                                                                                                  "cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb"))),
    "poolRegistrationOperatorMultipleOwnersAllRelays": Certificate(CertificateType.STAKE_POOL_REGISTRATION,
                                                        PoolRegistrationParams(poolKeys["poolKeyPath"],
                                                                               "07821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d084450",
                                                                               50000000000,
                                                                               340000000,
                                                                               Margin(3, 100),
                                                                               poolKeys["poolRewardAccountPath"],
                                                                               [poolKeys["stakingHashOwner0"],
                                                                                poolKeys["stakingHashOwner1"]],
                                                                               [relays["singleHostIPV4Relay0"],
                                                                                relays["singleHostIPV6Relay"],
                                                                                relays["singleHostNameRelay"],
                                                                                relays["multiHostNameRelay"]],
                                                                               PoolMetadataParams("https://www.vacuumlabs.com/sampleUrl.json",
                                                                                                  "cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb"))),
}

# =================
# signTx
# =================
testsByron = [
    SignTxTestCase("Sign tx with third-party Byron mainnet output",
                   Transaction(Mainnet,
                               [inputs["utxoByron"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258201af8fa0b754ff99253d983894e63a2b09cbb56c833ba18c3384210163f63dcfc00018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a",
                   SignedTransactionData([Witness("m/44'/1815'/0'/0/0",
                                                  "9c12b678a047bf3148e867d969fba4f9295042c4fff8410782425a356820c79e7549de798f930480ba83615a5e2a19389c795a3281a59077b7d37cd5a071a606")])),
    SignTxTestCase("Sign tx with third-party Byron Daedalus mainnet output",
                   Transaction(Mainnet,
                               [inputs["utxoByron"]],
                               [outputs["externalByronDaedalusMainnet"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258201af8fa0b754ff99253d983894e63a2b09cbb56c833ba18c3384210163f63dcfc00018182584c82d818584283581cd2348b8ef7b8a6d1c922efa499c669b151eeef99e4ce3521e88223f8a101581e581cf281e648a89015a9861bd9e992414d1145ddaf80690be53235b0e2e5001a199834651a002dd2e802182a030a",
                   SignedTransactionData([Witness("m/44'/1815'/0'/0/0",
                                                  "fdca7969a3e8bc091c9ee32c04732f79bb7c0091f1796fd2c0e1de8aa8547a00457d50d0576f4dd421baf754499cf0e77584e848e3547addd5d5b7167597a307")])),
    SignTxTestCase("Sign tx with third-party Byron testnet output",
                   Transaction(Testnet,
                               [inputs["utxoByron"]],
                               [outputs["externalByronTestnet"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258201af8fa0b754ff99253d983894e63a2b09cbb56c833ba18c3384210163f63dcfc00018182582f82d818582583581c709bfb5d9733cbdd72f520cd2c8b9f8f942da5e6cd0b6994e1803b0aa10242182a001aef14e76d1a002dd2e802182a030a",
                   SignedTransactionData([Witness("m/44'/1815'/0'/0/0",
                                                  "224d103185f4709f7b749339ff7ba432d50ca5cb742678847f5e574858cf7dda7ed402399a9ddba81ecd731b6f939ba07a247cd570dcd543f83a9aeadc4f9603")])),
]

testsShelleyNoCertificates = [
    SignTxTestCase("Sign tx without outputs",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018002182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "190dcee0cc7125fd0ec104cf685674f1ad77f3e439a4a249e596a3306f9eb110ced8fb8ec59da15b721203c8973bd341d88e6a60b85c1e9f2623152fee8dc00a")])),
    SignTxTestCase("Sign tx with 258 tag on inputs",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400d90102818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018002182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "b842908ce71f3ad1e1a1e2261c3bfdbfdb48c3fe58484c3e0521588e94e48fdb001f30908b0cd041e6c1b9d9400739ea52d0ca7289b3d807d26d06d73961f609")]),
                   True),
     SignTxTestCase("Sign tx without change address",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "ef73b0838e831dc86278e450ef36fecf4b7ad712dabb901f0d8470b6046ced8246cd086a15ad4c723c0cf01b685d8113e72a01511a5ceba374ebb8f4417afd0a")])),
    SignTxTestCase("Sign tx with change base address with staking path",
                   Transaction(Mainnet,
                               [inputs["utxoByron"]],
                               [outputs["externalByronMainnet"],
                                outputs["internalBaseWithStakingPath"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258201af8fa0b754ff99253d983894e63a2b09cbb56c833ba18c3384210163f63dcfc00018282582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e88258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a",
                   SignedTransactionData([Witness("m/44'/1815'/0'/0/0",
                                                  "4d29b3a66a152819baf9eb4866ab13ff6c5279ed80157463b96e2fd55aed14fa01d9df1de2a32560354da3db4f34cad79772804356401fa22523aabfd0363f03")])),
    SignTxTestCase("Sign tx with change base address with staking key hash",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"],
                                outputs["internalBaseWithStakingKeyHash"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018282582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e88258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f1124122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b42771a006ca79302182a030a",
                   SignedTransactionData([Witness("m/44'/1815'/0'/0/0",
                                                  "4ac5017c014886406a38a45417a165156280be63ca6975a5acffcabc0cc842ca603248b8a7ebfa729d7affce34518f4ca94fe797420a4d7aa0ef8c2b0ddfba0b")])),
    SignTxTestCase("Sign tx with enterprise change address",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"],
                                outputs["internalEnterprise"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018282582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e882581d6114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241a006ca79302182a030a",
                   SignedTransactionData([Witness("m/44'/1815'/0'/0/0",
                                                  "70559415746a9646dc492b7758e18cb367c005ab0479558b3d540be2310eb1bb1dd0839081e22c0b4727e8bd8e163cfbfe9def99a8506fb4a6787a200862e00f")])),
    SignTxTestCase("Sign tx with pointer change address",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"],
                                outputs["internalPointer"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018282582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e88258204114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11240102031a006ca79302182a030a",
                   SignedTransactionData([Witness("m/44'/1815'/0'/0/0",
                                                  "1838884e08cf6966ebe6b3e775191c4f08d90834723421779efd6aa96e52ffc91a24e5073abe6db94c74fe080d008258b3d989c159d9b87a9c778a51404abc08")])),
    SignTxTestCase("Sign tx with non-reasonable account and address",
                   Transaction(Mainnet,
                               [inputs["utxoNonReasonable"]],
                               [outputs["internalBaseWithStakingPathNonReasonable"]],
                               42,
                               10,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.ARBITRARY_HASH,
                                                             TxAuxiliaryDataHash(f"{'deadbeef'*8}"))),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182583901f90b0dfcace47bf03e88f7469a2f4fb3a7918461aa4765bfaf55f0dae260546c20562e598fb761f419dad27edcd49f4ee4f0540b8e40d4d51a006ca79302182a030a075820deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef",
                   SignedTransactionData([Witness("m/1852'/1815'/456'/0/0",
                                                  "bb1a035acf4a7b5dd68914f0007dfc4d1cc7b4d88748c0ad24326fd06597542ce0352075ed861b3ae012ab976cacd3dbbc58802cdf82409917ebf9a8bb182e04")])),
    SignTxTestCase("Sign tx with path based withdrawal",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               withdrawals=[Withdrawal(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                        "m/1852'/1815'/0'/2/0"),
                                                       111)]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a05a1581de11d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c186f",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "22ef3b54a54a1f5390436911b23328225f92c660eb251189fceab2fa428187a2cec584ea5f6f9c9fcdf7f19bc496b3b2b9bb416ad07a3d31d73fbc0c05bec10c"),
                                          Witness("m/1852'/1815'/0'/2/0",
                                                  "04b995979c2072b469c1e0ace5331c3d188e3e65d5a6f06aa4e608fb18a3588621370ee1b5d39d55afe0744aa4906785baa07210dc4cb49594eba507f7215102")])),
    SignTxTestCase("Sign tx with auxiliary data hash",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.ARBITRARY_HASH,
                                                             TxAuxiliaryDataHash(f"{'deadbeef'*8}"))),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a075820deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "953c5243ba09570dd4e52642236834c138ad4abbbb21796a90540a11e8dc96e47043401d370cdaed70ebc332dd4db80c9b167fd7f20971c4f142875cea57200c")])),
]

testsShelleyWithCertificates = [
    SignTxTestCase("Sign tx with a stake registration path certificate --- pre-Conway",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_REGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                  "m/1852'/1815'/0'/2/0")))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048182008200581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "9825594e5a91333b9f5762665ba316af34c2208bd7ef073178af5e48f2aae8673d50436045e292d5bb9be7492eeeda475a04e58621a326c91049a2ef26a33200")])),
    SignTxTestCase("Sign tx with a stake deregistration path certificate --- pre-Conway",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_DEREGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                  "m/1852'/1815'/0'/2/0")))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048182018200581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "6136510eb91449474f6137c8d1c7c69eb518e3844a3e63a626be8cf4af91afa24e12f4fa578398bf0e7992e22dcfc5f9773fb8546b88c19e3abfdaa3bbe7a304"),
                                          Witness("m/1852'/1815'/0'/2/0",
                                                  "77210ce6533a76db3673af1076bf3933747a8d81cabda80c8bc9c852c78685f8a42c9372721bdfe9b47611039364afb3391031211b5c427cfec0c5c505cfec0c")])),
    SignTxTestCase("Sign tx with a stake delegation path certificate",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_DELEGATION,
                                                         StakeDelegationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                "m/1852'/1815'/0'/2/0"),
                                                                               "f61c42cbf7c8c53af3f520508212ad3e72f674f957fe23ff0acb4973"))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048183028200581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c581cf61c42cbf7c8c53af3f520508212ad3e72f674f957fe23ff0acb4973",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "d94c8f8fe73946c25f3bd0919d05a60b8373ef0a7261fa73eefe1f2a20e8a4c3401feb5eea701222184fceab2c45b47bd823ac76123e2d17f804d3e4ed2df909"),
                                          Witness("m/1852'/1815'/0'/2/0",
                                                  "035b4e6ae6f7a8089f2a302ddcb60bc56d48bcf267fdcb071844da5ce3086d51e816777a6fb5eabfcb326a32b830674ac0de40ee1b2360a69adba4b64c662404")])),
    SignTxTestCase("Sign tx and filter out witnesses with duplicate paths",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_DEREGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                  "m/1852'/1815'/0'/2/0"))),
                                             Certificate(CertificateType.STAKE_DEREGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                  "m/1852'/1815'/0'/2/0")))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048282018200581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c82018200581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "04e071e39903e7e1e3ea9d26ce6822d5cbef88ee389f4f63a585668a5a6df98924dca16f8f61c01909162730014bb309fc7043b80ac54375697d6e9c01df0a0c"),
                                          Witness("m/1852'/1815'/0'/2/0",
                                                  "7b53ba805658d801baa39546777b611ed071c89938daea50c2c3275358abec2c1d67c8062b24fc4778e09af13e58ea33dd7d0627e221574386716aaa25e1f20b")])),
    SignTxTestCase("Sign tx with pool retirement combined with stake registration",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_POOL_RETIREMENT,
                                                         PoolRetirementParams("m/1853'/1815'/0'/0'", 10)),
                                             Certificate(CertificateType.STAKE_REGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                  "m/1852'/1815'/0'/2/0")))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a04828304581cdbfee4665e58c8f8e9b9ff02b17f32e08a42c855476a5d867c2737b70a82008200581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c",
                   # WARNING: only as computed by ledger, not verified with cardano-cli
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "8212cdabe1be514fdc21e02a2b405ce284ebbce0208a5c2b289dac662bf87fb4c2d18237c66761e285d78ee76cc26b7517718e641174d69f49737a49e9482607"),
                                          Witness("m/1853'/1815'/0'/2/0",
                                                  "9386c2545e2671497daf95db93be1386690a4f884547a60f2913ef8a9e61486ba068d7477e1cd712f8d9cc20778d9e71b72eda96c9394c2f3111c61803f9a70d")])),
    SignTxTestCase("Sign tx with pool retirement combined with stake deregistration",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_POOL_RETIREMENT,
                                                         PoolRetirementParams("m/1853'/1815'/0'/0'", 10)),
                                             Certificate(CertificateType.STAKE_DEREGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                          "m/1852'/1815'/0'/2/0")))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a04828304581cdbfee4665e58c8f8e9b9ff02b17f32e08a42c855476a5d867c2737b70a82018200581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c",
                   # WARNING: only as computed by ledger, not verified with cardano-cli
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "82bc899f008446e57daef9dc750d500d43f12fdd1e353d5ab8b42100a7bb94c9794de5e1ce03c06775e7da581f9cb08427e1d8b491d39ddfb3db060de3001700"),
                                          Witness("m/1853'/1815'/0'/0/0",
                                                  "4f58bfe90112eee3ce66edb7196506b5548c1c342619ee125e1f35fdbe009736593b3bfa80622727b6debc72626d60e3c4cb2d35007da9478baa4109dd80d004"),
                                          Witness("m/1852'/1815'/0'/2/0",
                                                  "1c926e24d825d699a4b2d5d7dc95d717d4c19a0196ed120f115c76a168a7e661e6c393c4f97fe7b7533f20017be834fae53711265a3fe52b4c4211ac18990007")])),
]

testsConwayWithCertificates = [
    SignTxTestCase("Sign tx with a stake registration path certificate --- Conway",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_REGISTRATION_CONWAY,
                                                         StakeRegistrationConwayParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                        "m/1852'/1815'/0'/2/0"),
                                                                                       17))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048183078200581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c11",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "128c993b5873029f98df738191462c6e8903ec2a765f7ddcc3a5722b5555e4ef2cccc4464bbdfb606627fe48e97f2db94f68c9d71b4076c93db682bd357ffa0b"),
                                          Witness("m/1852'/1815'/0'/2/0",
                                                  "bf22ca4b78fa64692a8cfbc375611deb1d60043db880ff000aeba7e4970492daa4e814c79960977816a91f9dd179bec6f127d37b8955589674e385b9a757d507")])),
    SignTxTestCase("Sign tx with a stake deregistration path certificate --- Conway",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_DEREGISTRATION_CONWAY,
                                                         StakeRegistrationConwayParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                        "m/1852'/1815'/0'/2/0"),
                                                         17))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048183088200581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c11",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "b0a196ce81930fedb90551d395f7ba2ff8671f1528d1eec85e3d2398174a1bf6c9bc2afc6c14891cf11a24e16c4a6d39f73689f5947170d0f0d9a53418c76400"),
                                          Witness("m/1852'/1815'/0'/2/0",
                                                  "c9ee07d3ee9ed1be5576e1ed371393335a98270f2a316ebec6c702e519db1b68a8cfb67039355a46a3f9d96051c74ae4e13e41d1ad05c3a401a8365369ea8407")])),
    SignTxTestCase("Sign tx with vote delegation certificates",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.VOTE_DELEGATION,
                                                         VoteDelegationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                               "m/1852'/1815'/0'/2/0"),
                                                                              DRepParams(DRepParamsType.KEY_PATH,
                                                                                         "m/1852'/1815'/0'/3/0"))),
                                             Certificate(CertificateType.VOTE_DELEGATION,
                                                         VoteDelegationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                               "m/1852'/1815'/0'/2/0"),
                                                                              DRepParams(DRepParamsType.KEY_HASH,
                                                                                         "7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8"))),
                                             Certificate(CertificateType.VOTE_DELEGATION,
                                                         VoteDelegationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                               "m/1852'/1815'/0'/2/0"),
                                                                              DRepParams(DRepParamsType.SCRIPT_HASH,
                                                                                         "1afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8"))),
                                             Certificate(CertificateType.VOTE_DELEGATION,
                                                         VoteDelegationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                               "m/1852'/1815'/0'/2/0"),
                                                                              DRepParams(DRepParamsType.ABSTAIN))),
                                             Certificate(CertificateType.VOTE_DELEGATION,
                                                         VoteDelegationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                               "m/1852'/1815'/0'/2/0"),
                                                                              DRepParams(DRepParamsType.NO_CONFIDENCE)))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048183088200581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c11",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "1a9dd087bce2b189a1d2a3ff6e57017bf6cef86d51ca944a8faf9c04cddafd4336e4bdebc29450c82b766f766b4a7982b5cee9731edb85f9025c7826880de106"),
                                          Witness("m/1852'/1815'/0'/2/0",
                                                  "8100907b358d25330003ea0f9606c031256f2ca060322138e3e118676cdea4e949b1a2217e714f6c5686a31fe70e80bcb2d460f8b7f12a7f5926c1211502c70f")])),
    SignTxTestCase("Sign tx with AUTHORIZE_COMMITTEE_HOT certificates",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.AUTHORIZE_COMMITTEE_HOT,
                                                         AuthorizeCommitteeParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                   "m/1852'/1815'/0'/4/0"),
                                                                                  CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                   "m/1852'/1815'/0'/5/0"))),
                                             Certificate(CertificateType.AUTHORIZE_COMMITTEE_HOT,
                                                         AuthorizeCommitteeParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                   "m/1852'/1815'/0'/4/0"),
                                                                                  CredentialParams(CredentialParamsType.KEY_HASH,
                                                                                                   "1afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8"))),
                                             Certificate(CertificateType.AUTHORIZE_COMMITTEE_HOT,
                                                         AuthorizeCommitteeParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                   "m/1852'/1815'/0'/4/0"),
                                                                                   CredentialParams(CredentialParamsType.SCRIPT_HASH,
                                                                                                    "1afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8")))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a0483830e8200581ccf737588be6e9edeb737eb2e6d06e5cbd292bd8ee32e410c0bba1ba68200581cd098c6a0a621f3343abe55877ee88fd5a83363e3c7887b3c48839092830e8200581ccf737588be6e9edeb737eb2e6d06e5cbd292bd8ee32e410c0bba1ba68200581c1afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8830e8200581ccf737588be6e9edeb737eb2e6d06e5cbd292bd8ee32e410c0bba1ba68201581c1afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "73e3c0105404e90f6cccebbd9a1e9bb119c4f0a3d5e489a52d15caa477db35802fefe83dbec748b2c388b9dc193821eac068f519edb89ed2c550bc28e51a7203"),
                                          Witness("m/1852'/1815'/0'/4/0",
                                                  "b8075d9666648e02f3e2ab6355fd786b3864cfc18a0070beb60813745a36c68e03b9face68231efc0d13da11069f2b3c7ca9a2c7fbf2119970071607e5c18d01")])),
    SignTxTestCase("Sign tx with RESIGN_COMMITTEE_COLD certificates",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.RESIGN_COMMITTEE_COLD,
                                                         ResignCommitteeParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                "m/1852'/1815'/0'/4/0"),
                                                                               AnchorParams(f"{'x'*128}",
                                                                                            "1afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef"))),
                                             Certificate(CertificateType.RESIGN_COMMITTEE_COLD,
                                                         ResignCommitteeParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                "m/1852'/1815'/0'/4/0")))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a0482830f8200581ccf737588be6e9edeb737eb2e6d06e5cbd292bd8ee32e410c0bba1ba6827880787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787878787858201afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef830f8200581ccf737588be6e9edeb737eb2e6d06e5cbd292bd8ee32e410c0bba1ba6f6",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "82f826cea558d08e833e3657f6afaba0175813a7de5bda7c0e075f4b08a2833a6e61f7a2587d9b88e919b4b4aa465cc689a6a611c8cf96fe1249f47ffd22ba05"),
                                          Witness("m/1852'/1815'/0'/4/0",
                                                  "6eff66993a624cb218bf84dbdd3481212b4586bf1d8ef0a9c6e5bf32c977afbb1e09a1c0f3810f6828b17f9d4f3acee7d59af098e98d46d9a95d5d60fd9a8f07")])),
    SignTxTestCase("Sign tx with DREP_REGISTRATION certificates",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.DREP_REGISTRATION,
                                                         DRepRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                 "m/1852'/1815'/0'/3/0"),
                                                                                19,
                                                                                AnchorParams("www.vacuumlabs.com",
                                                                                             "1afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef"))),
                                             Certificate(CertificateType.DREP_REGISTRATION,
                                                         DRepRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                 "m/1852'/1815'/0'/3/0"),
                                                                                19))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048284108200581cba41c59ac6e1a0e4ac304af98db801097d0bf8d2a5b28a54752426a11382727777772e76616375756d6c6162732e636f6d58201afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef84108200581cba41c59ac6e1a0e4ac304af98db801097d0bf8d2a5b28a54752426a113f6",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "ce8634b0306f71be765b655488276e86a56cbe6e77ffdd14e40851394cf6842d695f4184df0dcb8e76dd5918ec9b1b65976b31bf54c5621b34b1ff58d631540d"),
                                          Witness("m/1852'/1815'/0'/3/0",
                                                  "d1600ce316bac100729207b08550d53f77834983fa4ae1285d8b8478d813cb8a9321588026e40521147718ba3708c533e4b37f890bc9b66ac0456534e926ce06")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with DREP_DEREGISTRATION certificates",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.DREP_DEREGISTRATION,
                                                         DRepRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                 "m/1852'/1815'/0'/3/0"),
                                                                                19))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048183118200581cba41c59ac6e1a0e4ac304af98db801097d0bf8d2a5b28a54752426a113",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "903e42d8f48c60d4bacc8aa953bff66c79ef8b48c2eece243f03d32146f9d213e6595809bc6c13e2ebb13f990ad0ef7bb937eab6a9d251c35aae7bafc2c51702"),
                                          Witness("m/1852'/1815'/0'/3/0",
                                                  "56786aa0710832c8ef584ebe964473cd21eb15b4b60057a5d96973f6a4687176d1f7cbecde585f7b875fc75ad73a9404733219d61ee542363ef4baa8a1bb1504")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with DREP_UPDATE certificates",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.DREP_UPDATE,
                                                         DRepUpdateParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                           "m/1852'/1815'/0'/3/0"),
                                                                         AnchorParams("www.vacuumlabs.com",
                                                                                      "1afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef"))),
                                             Certificate(CertificateType.DREP_UPDATE,
                                                         DRepUpdateParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                           "m/1852'/1815'/0'/3/0")))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048283128200581cba41c59ac6e1a0e4ac304af98db801097d0bf8d2a5b28a54752426a182727777772e76616375756d6c6162732e636f6d58201afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef83128200581cba41c59ac6e1a0e4ac304af98db801097d0bf8d2a5b28a54752426a1f6",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "e65f9e0d99db02f1a39b8af92b9f7e8f6509b24cbd6f4492e19d699f5f7e08d627a64def7408405ff0aadd7504bacfa279a2f1ba550a765f43d06fcac16f1009"),
                                          Witness("m/1852'/1815'/0'/3/0",
                                                  "88b20b4bc43f6d45981c658689e6d80886b6aeb02b654363c421462f855654039d5ff3595be6778934b4eff2a24c3a082789f95559bd6e8afa2e8bab339e910d")])),
]

testsMultisig = [
    SignTxTestCase("Sign tx without change address with Shelley scripthash output",
                   Transaction(Testnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalShelleyBaseScripthashKeyhash"]],
                               42,
                               10),
                   TransactionSigningMode.MULTISIG_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e0102182a030a",
                   SignedTransactionData([Witness("m/1854'/1815'/0'/0/0",
                                                  "9c06b237c35be528a3f550e469e38c32c29a58417d489d8d4f1276a2111b2f6feca9b84d658f5e51ee7921512fe935e11defc7a1ff6152f76ea590baca04f307")]),
                   additionalWitnessPaths=["m/1854'/1815'/0'/0/0"]),
    SignTxTestCase("Sign tx with script based withdrawal",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               withdrawals=[Withdrawal(CredentialParams(CredentialParamsType.SCRIPT_HASH,
                                                                        "122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277"),
                                                       111)]),
                   TransactionSigningMode.MULTISIG_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a05a1581df1122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277186f",
                   SignedTransactionData([Witness("m/1854'/1815'/0'/2/0",
                                                  "64f26fb866f0840f2ec299db16e6eff9d039ebacf673bdd8dba5110078344bf9647c4038588bfc826c73d7e0c03ea2ffb028de632d9462a129fd78f3a1bd7c0e")]),
                   additionalWitnessPaths=["m/1854'/1815'/0'/2/0"]),
    SignTxTestCase("Sign tx with a stake registration script certificate",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_REGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.SCRIPT_HASH,
                                                                                                  "122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277")))]),
                   TransactionSigningMode.MULTISIG_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048182008201581c122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277",
                   SignedTransactionData([Witness("m/1854'/1815'/0'/2/0",
                                                  "bfb96452a106da86ff17c71692e25fac4826ae1c318c94d671fd7602229b411cf4422614cba241954a9bdb66bfd364bc9cfdf446639ff6e03273dc4073d66b0a")]),
                   additionalWitnessPaths=["m/1854'/1815'/0'/2/0"]),
    SignTxTestCase("Sign tx with a stake delegation script certificate",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_DELEGATION,
                                                         StakeDelegationParams(CredentialParams(CredentialParamsType.SCRIPT_HASH,
                                                                                                "122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277"),
                                                                               "f61c42cbf7c8c53af3f520508212ad3e72f674f957fe23ff0acb4973"))]),
                   TransactionSigningMode.MULTISIG_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048183028201581c122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277581cf61c42cbf7c8c53af3f520508212ad3e72f674f957fe23ff0acb4973",
                   SignedTransactionData([Witness("m/1854'/1815'/0'/2/0",
                                                  "78de23f120ff291913eee3d3981281d500e9476debb27bb640ff73eba53c1de452b5d9dba57d4353a37652f7a72a272e60a928fbf4181b70c031c9ba93888606")]),
                   additionalWitnessPaths=["m/1854'/1815'/0'/2/0"]),
    SignTxTestCase("Sign tx with a stake deregistration script certificate",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_DEREGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.SCRIPT_HASH,
                                                                                                  "122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277")))]),
                   TransactionSigningMode.MULTISIG_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a048182018201581c122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277",
                   SignedTransactionData([Witness("m/1854'/1815'/0'/2/0",
                                                  "468e5dc048efa4985bb392248f6d8df3b4ed297a9cbe4b9670ac0cc0debc4e6dc00018a75079cf20c050f4bf9be1c9aecccae851d22fe940a72b25af802d910b")]),
                   additionalWitnessPaths=["m/1854'/1815'/0'/2/0"]),
]

testsAllegra = [
    SignTxTestCase("Sign tx with no ttl and no validity interval start",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a300818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "4f6e3a61b48921fa0c3f67856fc955e754d16d210f0725ff31d959c53f830ddef354663040bc0bc4306127c3549f0c5339cc5a604512090a4fe26ebadc80550f")])),
    SignTxTestCase("Sign tx with no ttl , but with validity interval start",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               validityIntervalStart=47),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a08182f",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "e245b89938ad182361d696dafb0644b6b93bcfa3e631716afb5f73b6b6d6852c9313d7fd34a4a404e4b345b64d9b29ddef406197911106593000cd2fd18b900f")])),
]

testsMary = [
    SignTxTestCase("Sign tx with a multiasset output",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["multiassetOneToken"],
                                outputs["internalBaseWithStakingPath"]],
                               42,
                               10,
                               validityIntervalStart=7),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018282583901eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff821904d2a1581c95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a14874652474436f696e1a007838628258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a0807",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "6c4d490ea8f3973f9d030c36ff7221f012663af276bde346f8b90b54b06f49c22bcde3968cc281d548183e1506380028853948f7ef3c98a9e179540119688106")])),
    SignTxTestCase("Sign tx with a complex multiasset output",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["multiassetManyTokens"],
                                outputs["internalBaseWithStakingPath"]],
                               42,
                               10,
                               validityIntervalStart=7),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018282583901eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff821904d2a2581c7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373a34003581c1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df209015820000000000000000000000000000000000000000000000000000000000000000002581c95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a248456c204e69c3b16f1904d24874652474436f696e1a007838628258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a0807",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "e2c96040194d1baef4b9dfac945496f60d446597863a2049d12796df7fb6f9f9f31392555cfccfd7c745eef802d1904ba3a9ba4892569d0eed6f6e19a871630f")])),
    SignTxTestCase("Sign tx with big numbers",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["multiassetBigNumber"]],
                               24103998870869519,
                               24103998870869519,
                               validityIntervalStart=24103998870869519),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182583901eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff821b0055a275925d560fa1581c95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a14874652474436f696e1b0055a275925d560f021b0055a275925d560f031b0055a275925d560f081b0055a275925d560f",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "632cd935550a71c1e1869e6f5749ee4cb8c268cbe014138561fc2d1045b5b2be84526cfd5a6fea01de99bdf903fa17c79a58a832b5cdcb1c999bcbe995a56806")])),
    SignTxTestCase("Sign tx with a multiasset change output",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"],
                                outputs["multiassetChange"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000182825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b09018258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c821904d2a1581c95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a14874652474436f696e1a0078386202182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "7d437d698a9a8f06f1e0ced7378c22b864b4a3dd8bba575e5cc497f55fcee984724549a34cb6e5ea11acf4749544ddabf2118c0545c668c5f75251a6be443905")])),
    SignTxTestCase("Sign tx with zero fee, TTL and validity interval start",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPath"]],
                               0,
                               0,
                               validityIntervalStart=0),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca793020003000800",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "e5ee59942fba139b5547e5e1dae1389ed9edd6e7bd7f057b988973c2451b5e3e41901c1d9a0fa74d34dae356a064ee783205d731fee01105c904702826b66b04")])),
    SignTxTestCase("Sign tx with output with decimal places",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["multiassetDecimalPlaces"]],
                               33),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a300818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182583901eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff821904d2a2581c6954264b15bc92d6d592febeac84f14645e1ed46ca5ebb9acdb5c15fa14553545249501a0034bf15581caf2e27f580f7f08e93190a81f72462f153026d06450924726645891ba244445249501904d24cffffffffffffffffffffffff1904d2021821",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "30e8da0b9230bc1b1e2748ef51e9259f457d4e0bd0387eb186ade839f3bbac5a2face7eea72061b850c7d26a5b66bd0f90cff546c6c30e0987091a067c960d06")])),
    SignTxTestCase("Sign tx with mint fields with various amounts",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [],
                               42,
                               10,
                               mint=mints["mintAmountVariety"]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018002182a030a09a1581c7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373a44000581c1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df20920581c1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df20a1b7fffffffffffffff581c1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df20b3b7fffffffffffffff",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "18fa055fb6d74b12170cdc227aaf4922c78405d4caf7bdbe5f959df2c3a912e20c5a18c4412d504685fe1179d32b5b588efe4a8d59f0274492de77f30f315409")])),
    SignTxTestCase("Sign tx with mint fields with mint with decimal places",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               33,
                               mint=mints["mintWithDecimalPlaces"]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182109a2581c6954264b15bc92d6d592febeac84f14645e1ed46ca5ebb9acdb5c15fa14553545249503a0034bf14581caf2e27f580f7f08e93190a81f72462f153026d06450924726645891ba244445249501904d24cffffffffffffffffffffffff1904d2",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "11b9ed90e2923c01869627ed5bc49ea66874fbef2418a2184437e19a30738a8bb52d7569113984617d73144e304be5cf84a30c21bd8b1c4cfe93cc434ed3db04")])),
    SignTxTestCase("Sign tx with mint fields among other fields",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["multiassetOneToken"],
                                outputs["internalBaseWithStakingPath"]],
                               10,
                               1000,
                               validityIntervalStart=100,
                               mint=mints["mintAmountVariety"]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018282583901eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff821904d2a1581c95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a14874652474436f696e1a007838628258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca793020a031903e808186409a1581c7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373a44000581c1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df20920581c1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df20a1b7fffffffffffffff581c1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df20b3b7fffffffffffffff",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "2a4ec4e5eb03d24264d612923e62b01384d215a70c415b067cc109580cef1044fc9a5b17fe92f752b70702fd457e6ea455a4ef5f3afdd44548223e913bc43b08")])),
]

testsAlonzoTrezorComparison = [
    SignTxTestCase("Full test for trezor feature parity",
                   Transaction(Mainnet,
                               [inputs["utxoMultisig"]],
                               [outputs["trezorParity1"],
                                outputs["trezorParityDatumHash1"]],
                               42,
                               10,
                               validityIntervalStart=47,
                               certificates=[Certificate(CertificateType.STAKE_REGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.SCRIPT_HASH,
                                                                                                  "29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd"))),
                                             Certificate(CertificateType.STAKE_DEREGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.SCRIPT_HASH,
                                                                                                  "29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd"))),
                                             Certificate(CertificateType.STAKE_DELEGATION,
                                                         StakeDelegationParams(CredentialParams(CredentialParamsType.SCRIPT_HASH,
                                                                                                "29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd"),
                                                                               "f61c42cbf7c8c53af3f520508212ad3e72f674f957fe23ff0acb4973"))],
                               withdrawals=[Withdrawal(CredentialParams(CredentialParamsType.SCRIPT_HASH,
                                                                        "29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd"),
                                                       1000)],
                               mint=mints["trezorComparison"],
                               includeNetworkId=True,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.ARBITRARY_HASH,
                                                             TxAuxiliaryDataHash("58ec01578fcdfdc376f09631a7b2adc608eaf57e3720484c7ff37c13cff90fdf")),
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7"),
                   TransactionSigningMode.MULTISIG_TRANSACTION,
                   "ab00818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018282583901eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff821a001e8480a1581c0d63e8d2c5a00cbcffbdf9112487c443466e1ea7d8c834df5ac5c425a14874657374436f696e1a0078386283581d71477e52b3116b62fe8cd34a312615f5fcd678c94e1d6cdb86c1a3964c0158203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b702182a030a048382008201581c29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd82018201581c29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd83028201581c29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd581cf61c42cbf7c8c53af3f520508212ad3e72f674f957fe23ff0acb497305a1581df129fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd1903e807582058ec01578fcdfdc376f09631a7b2adc608eaf57e3720484c7ff37c13cff90fdf08182f09a1581c0d63e8d2c5a00cbcffbdf9112487c443466e1ea7d8c834df5ac5c425a24874657374436f696e1a007838624875657374436f696e3a007838610b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70f01",
                   SignedTransactionData([Witness("m/1854'/1815'/0'/0/0",
                                                  "0d35e3f273db757d6137ff897dcfe5abf44054185a428197933a61bb0c7ad960c2090ba808ab86404fe2b407abba12041f5e9306a6f1ae0ad5b6cd4fc7b36904"),
                                          Witness("m/1854'/1815'/0'/2/0",
                                                  "8100907b358d253a164b873fa4678dc7a986ad9e4db62b638faff7f45c81af835155bc74dd3ad4b2f696734bf1e536de2baa237f92e158624920eb10269f9ee1d9910993b194a0b30003ea0f9606c031256f2ca060322138e3e118676cdea4e949b1a2217e714f6c5686a31fe70e80bcb2d460f8b7f12a7f5926c1211502c70f")]),
                   additionalWitnessPaths=["m/1854'/1815'/0'/0/0", "m/1854'/1815'/0'/2/0"]),
]

testsBabbageTrezorComparison = [
    SignTxTestCase("Full test for trezor feature parity - Babbage elements (Plutus)",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["trezorParity2"],
                                outputs["trezorParityDatumHash2"]],
                               42,
                               10,
                               validityIntervalStart=47,
                               includeNetworkId=True,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                               collateralInputs=[inputs["utxoShelley"]],
                               collateralOutput=outputs["externalShelleyBaseKeyhashKeyhash"],
                               totalCollateral=10,
                               referenceInputs=[inputs["utxoShelley"]]),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "ab00818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000182825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b09821a001e8480a1581c0d63e8d2c5a00cbcffbdf9112487c443466e1ea7d8c834df5ac5c425a14874657374436f696e1a0078386283581d71477e52b3116b62fe8cd34a312615f5fcd678c94e1d6cdb86c1a3964c0158203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b702182a030a08182f0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70d818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000f0110825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b0901110a12818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "b6625562153024a481503905b9d05b9e3b6c1b5267f2cce6531e93a14052a1c7db6cc799d77c3ce4f1efd5b7b199c28af4aca6ca59d0d1423eac476e748a8901")])),
    SignTxTestCase("Full test for trezor feature parity - Babbage elements (ordinary)",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["trezorParityBabbageOutputs"]],
                               42,
                               10,
                               validityIntervalStart=47,
                               includeNetworkId=True),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a400581d71477e52b3116b62fe8cd34a312615f5fcd678c94e1d6cdb86c1a3964c0101028201d818565579657420616e6f746865722063686f636f6c61746503d81858390080f9e2c88e6c817008f3a812ed889b4a4da8e0bd103f86e7335422aa122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b427702182a030a08182f0f01",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "a655f96ffc6fe56c5f9287dc28474805a97a46b85def8002cb1d4ee975fe69ae89a4a24c317db9cc3c7390410465ded89f349d081de5fd757689af9b6c125609")])),
]

testsMultidelegation = [
    SignTxTestCase("Sign tx with multidelegation keys in all tx elements",
                   Transaction(Mainnet,
                               [inputs["utxoShelley2"]],
                               [outputs["trezorParity1"],
                                outputs["trezorParityDatumHash1"]],
                               42,
                               10,
                               validityIntervalStart=47,
                               certificates=[Certificate(CertificateType.STAKE_REGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                  "m/1852'/1815'/0'/2/2"))),
                                             Certificate(CertificateType.STAKE_DEREGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                  "m/1852'/1815'/0'/2/2"))),
                                             Certificate(CertificateType.STAKE_DELEGATION,
                                                         StakeDelegationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                "m/1852'/1815'/0'/2/2"),
                                                                               "f61c42cbf7c8c53af3f520508212ad3e72f674f957fe23ff0acb4973"))],
                               withdrawals=[Withdrawal(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                        "m/1852'/1815'/0'/2/3"),
                                                       1000)],
                               requiredSigners=[RequiredSigner(TxRequiredSignerType.PATH, "m/1852'/1815'/0'/2/4")],
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                               includeNetworkId=True),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "aa00818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018282583901eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff821a001e8480a1581c0d63e8d2c5a00cbcffbdf9112487c443466e1ea7d8c834df5ac5c425a14874657374436f696e1a0078386283581d71477e52b3116b62fe8cd34a312615f5fcd678c94e1d6cdb86c1a3964c0158203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b702182a030a048382008200581cee6d266f2b60add5a249a3754f91cf1f423ac94c6cd964b3814f21a382018200581cee6d266f2b60add5a249a3754f91cf1f423ac94c6cd964b3814f21a383028200581cee6d266f2b60add5a249a3754f91cf1f423ac94c6cd964b3814f21a3581cf61c42cbf7c8c53af3f520508212ad3e72f674f957fe23ff0acb497305a1581de198acedf1c6b691f963d928147f66697c7cda3899e30c613037a4e9901903e808182f0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70e81581c86df572e0e28bec8ca8066e9d8c3681b4ac86c43c57cd52eb06ae8640f01",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/2/1",
                                                  "873f55cd0ffa058d1c93fed2eb748f33c12fc2f144cb493e7e621eb2a248c809f43208e31e53ac85a3186a9c848d7b4aa95928635355a2f4473a3216e0466407"),
                                          Witness("m/1852'/1815'/0'/2/2",
                                                  "c04a246ae7495f9e0733b5610dcac3ee955b417dc6089dd36da714566230235bd5aa7ff6f364477b32d48d1bee5764166a4c0ecb5c13946d7d1f1b2611f5fc00"),
                                          Witness("m/1852'/1815'/0'/2/3",
                                                  "1c9c5d5bccbc6a45adb41fde16fb82b5ba2748755ce048a2b50e1994d59f51905b2e43a8eef886d62ee1f77e81df201569bb1bab39c14f80c71a0568fcbf5e02"),
                                          Witness("m/1852'/1815'/0'/2/4",
                                                  "4a1026097d9a2ab2ead55cab8979e29bcf2ead8b7a2141c1175f8bc2ed54f3333435455b552e04ef0e3f25b943d63a682da40088a812088c229e9afb3af6b806"),
                                          Witness("m/1852'/1815'/0'/0/0",
                                                  "531e53866638657fabdd20315f1639a59887c3db538b4917e5bea0a9f20ca66908848a279e47dbc1be3a880fc4052272be106f3fa2a1e3d6fc300c5e92405106"),
                                          Witness("m/1852'/1815'/0'/2/5",
                                                  "af7bb2a9740c4a3de2ec239850523a43dd0689a1a73fc11dce399bfbfbf527c140d5751b45fc8bb733be2eab4610f56e947190c5dcbd0b72e3b236388cd17608")]),
                   additionalWitnessPaths=["m/1852'/1815'/0'/0/0", "m/1852'/1815'/0'/2/5"]),
]

testsConwayWithoutCertificates = [
    SignTxTestCase("Sign tx with treasury",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               treasury=27),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a15181b",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "d709944dbc56080b194455a76474981e56c64715b8e5182a58f9f5bba20357f2e02945431145e6fe418b1953424ef1b88e3328f373da1d24cb164d6eb8e0a80f")])),
    SignTxTestCase("Sign tx with donation",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               donation=28),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a16181c",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "eef7538ecd01708a3483900155c3c1545188b671f626a4622864cbfe41a51bfd4c47ccb9ee8accd7891bd0291bce3e44c8363a30ffbe80864af1a29a74239c00")])),
    SignTxTestCase("Sign tx with treasury and donation",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               treasury=27,
                               donation=28),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a15181b16181c",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "90627003619a5512c5600542bfe5ece07909433948a601d599f51bc0b3d19b5c8084cf72d2b6ae6962918392c540d60105c32626804986e99364e6966f5e1504")])),
]

vote1 = Vote(GovActionId("3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                         3),
             VotingProcedure(VoteOption.ABSTAIN,
                             AnchorParams("www.vacuumlabs.com",
                                          "1afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef")))
vote2 = Vote(GovActionId("3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                         3),
             VotingProcedure(VoteOption.NO))
vote3 = Vote(GovActionId("3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                         3),
             VotingProcedure(VoteOption.YES))

testsConwayVotingProcedures = [
    SignTxTestCase("Sign tx with voting procedures, COMMITTEE_KEY_PATH voter",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               votingProcedures=[VoterVotes(Voter(VoterType.COMMITTEE_KEY_PATH,
                                                                 "m/1852'/1815'/0'/5/0"),
                                                            [vote1])]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a13a18200581cd098c6a0a621f3343abe55877ee88fd5a83363e3c7887b3c48839092a18258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b703820282727777772e76616375756d6c6162732e636f6d58201afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "740d2c38e72b017566ee04cce0329795df75b9c5b5ab40045faabaafba16e80894169467a62db6e6a7110e9c3591819e9ba023c40598309c1db401ea6b84d107"),
                                          Witness("m/1852'/1815'/0'/5/0",
                                                  "87045d01fed7fe418601fd529ada773e40e48973760a10bff18702b76ad50d689cc17f350faba86aeea587441d3bc0856c3d650644cdf67e00ebdf4be9768f0c")])),
    SignTxTestCase("Sign tx with voting procedures, DREP_KEY_PATH voter",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               votingProcedures=[VoterVotes(Voter(VoterType.DREP_KEY_PATH,
                                                                 "m/1852'/1815'/0'/3/0"),
                                                            [vote2])]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a13a18202581cba41c59ac6e1a0e4ac304af98db801097d0bf8d2a5b28a54752426a1a18258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7038200f6",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "8ed6756e5913a5cf965fda822a61c0f1691e795709104194483915d383b3ed4dbeae8c1258fd4fa2a915e0a573c3a8fe5ee2bf1c56652e02f2729cdea7f43a03"),
                                          Witness("m/1852'/1815'/0'/3/0",
                                                  "901cb9f18ada4c07fb7b132fc7859d6e1a3beb42934f58d4d44fb8fbeef066a349482d4c159ecd54b61b4cb34e4db1fc5c57cc3b6bb4a040cf61d6dd88f73901")])),
    SignTxTestCase("Sign tx with voting procedures, STAKE_POOL_KEY_PATH voter",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               votingProcedures=[VoterVotes(Voter(VoterType.STAKE_POOL_KEY_PATH,
                                                                 "m/1853'/1815'/0'/0'"),
                                                            [vote3])]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a13a18204581cdbfee4665e58c8f8e9b9ff02b17f32e08a42c855476a5d867c2737b7a18258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7038201f6",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "8fcacfc066da36cab795cc820433fc4cef8cc0b8f3b62fb95a32da1056aea878411e8f6b891d1f77881f1b95903fb6c4fd4c3922b44de7dc790909a1d1dcb800"),
                                          Witness("m/1853'/1815'/0'/0'",
                                                  "745e2b96ae05133dba57695aff7a37e35a2c21f3c53174f280df5661974626839ed7cf6362a75db50349c30e935e2993a8493ef2a600e96fc67542cede5bbf02")])),
    SignTxTestCase("Sign tx with voting procedures, COMMITTEE_KEY_HASH voter",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               votingProcedures=[VoterVotes(Voter(VoterType.COMMITTEE_KEY_HASH,
                                                                 "7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8"),
                                                            [vote1])]),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a13a18200581c7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8a18258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b703820282727777772e76616375756d6c6162732e636f6d58201afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "6d4d4be3cae6d5cac5721fe29b6bbc445bfce24b6ebc6be663043e240a575a96238e0beea8391a080fb060e290b7661413c275ef3ba0458af86df30ed62f6202")])),
    SignTxTestCase("Sign tx with voting procedures, COMMITTEE_SCRIPT_HASH voter",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               votingProcedures=[VoterVotes(Voter(VoterType.COMMITTEE_SCRIPT_HASH,
                                                                 "7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8"),
                                                            [vote2])]),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a13a18201581c7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8a18258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7038200f6",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "803bd1793c4afdf303d9ae5e7ec2eef7344663f0cfc77a5f8327fb51c693d6a13f07066cb01ab12d575a1fc45f2747ff3a2302edd5048a74b6e205bed0384e00")])),
    SignTxTestCase("Sign tx with voting procedures, DREP_KEY_HASH voter",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               votingProcedures=[VoterVotes(Voter(VoterType.DREP_KEY_HASH,
                                                                 "7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8"),
                                                            [vote3])]),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a13a18202581c7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8a18258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7038201f6",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "7d8002af012f8ec0b67b3679f80f9119794062827834ded271be2e014970dd4206d6a53936d557daa74996d85812d6f541bad9578a05927d0c0778d63463f409")])),
    SignTxTestCase("Sign tx with voting procedures, DREP_SCRIPT_HASH voter",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               votingProcedures=[VoterVotes(Voter(VoterType.DREP_SCRIPT_HASH,
                                                                 "7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8"),
                                                            [vote1])]),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a13a18203581c7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8a18258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b703820282727777772e76616375756d6c6162732e636f6d58201afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "efd40f0cb90f93b3c664a1a8aba72941c570c07351db43054a399be4b0020d052397c8fdc28f2ed949d29b0d2040d5c296f447c0e104ed6ae58c553405e8c202")])),
    SignTxTestCase("Sign tx with voting procedures, STAKE_POOL_KEY_HASH voter",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               votingProcedures=[VoterVotes(Voter(VoterType.STAKE_POOL_KEY_HASH,
                                                                 "7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8"),
                                                            [vote1])]),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a13a18204581c7afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8a18258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b703820282727777772e76616375756d6c6162732e636f6d58201afd028b504c3668102b129b37a86c09a2872f76741dc7a68e2149c8deadbeef",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "5e9b164e24e0752c1c9987df83aa4ff9a54455c799231c191e8b150ad7af83db36cd5f91941f65b853f883b6721e4b9f724788fffa2371aaf5644ca53ccc1308")])),
]

# =================
# signTxCVote
# =================
testsCatalystRegistration = [
    SignTxTestCase("Sign tx with Catalyst registration metadata with base address",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPath"]],
                               42,
                               10,
                               validityIntervalStart=7,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.CIP36_REGISTRATION,
                                                             TxAuxiliaryDataCIP36(CIP36VoteRegistrationFormat.CIP_15,
                                                                                  "m/1852'/1815'/0'/2/0",
                                                                                  destinations["internalBaseWithStakingPath"],
                                                                                  1454448,
                                                                                  "4b19e27ffc006ace16592311c4d2f0cafc255eaa47a6178ff540c0a46d07027c"))),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a075820e9141b460aea0abb69ce113c7302c7c03690267736d6a382ee62d2a53c2ec9260807",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "b0bc6b3ddc0ab65e5b2e83cfdedbbf76619c3a833705f634f1c8c335dc7c1c5372ec7ebb8199d6d18204da4a0168a172c41c6dd53f45235225f5e62b672ca709")])),
   SignTxTestCase("Sign tx with Catalyst registration metadata with stake address",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPath"]],
                               42,
                               10,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.CIP36_REGISTRATION,
                                                             TxAuxiliaryDataCIP36(CIP36VoteRegistrationFormat.CIP_15,
                                                                                  "m/1852'/1815'/0'/2/0",
                                                                                  destinations["paymentKeyPath"],
                                                                                  1454448,
                                                                                  "4b19e27ffc006ace16592311c4d2f0cafc255eaa47a6178ff540c0a46d07027c"))),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a075820d19f7cb4d48a6ae8d370c64d2a42fca1f61d6b2cf3d0c0c02801541811338deb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "138afde8640bd8d1a08309455f604d842d65a85e5ce2f584974f004e9043dea670ead5de3e4895a320f94033d5476d56ccf7147f327156cc30aef8304c66c006")]),
                   nano_skip=True),
]

testsCVoteRegistrationCIP36 = [
   SignTxTestCase("Sign tx with CIP36 registration with vote key hex",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPath"]],
                               42,
                               10,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.CIP36_REGISTRATION,
                                                             TxAuxiliaryDataCIP36(CIP36VoteRegistrationFormat.CIP_36,
                                                                                  "m/1852'/1815'/0'/2/0",
                                                                                  destinations["paymentKeyPath"],
                                                                                  1454448,
                                                                                  "4b19e27ffc006ace16592311c4d2f0cafc255eaa47a6178ff540c0a46d07027c"))),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a0758201999b3bb9102b585c42616e40cf1290518d788f967ab4b3329dcb712ac933da0",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "60e3674421e004efcb6c893ec69a7131d52688cd927512510d59d83280639af55cbc05ae75bf7711d2562c26fa966ca17e908664c6fa7a042b7aac5a7f32d80d")]),
                   nano_skip=True),
   SignTxTestCase("Sign tx with CIP36 registration with vote key path",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPath"]],
                               42,
                               10,
                               validityIntervalStart=7,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.CIP36_REGISTRATION,
                                                             TxAuxiliaryDataCIP36(CIP36VoteRegistrationFormat.CIP_36,
                                                                                  "m/1852'/1815'/0'/2/0",
                                                                                  destinations["internalBaseWithStakingPath"],
                                                                                  1454448,
                                                                                  "m/1694'/1815'/0'/0/1"))),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a075820d05698c555a117014a3b360a66931ec43bf18e2aa16560fc99dbd92dd7f6f6540807",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "12cc08ec7f2047970a03ce68148392bceb870bc4735cda7ad3eb1eca17e7f5938d7790c8d51b2fcc7c1fd71571ea9fdee0f9a2702942fdd2e38bfc3573e5bf0f")]),
                   nano_skip=True),
   SignTxTestCase("Sign tx with CIP36 registration with unusual vote key path",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPath"]],
                               42,
                               10,
                               validityIntervalStart=7,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.CIP36_REGISTRATION,
                                                             TxAuxiliaryDataCIP36(CIP36VoteRegistrationFormat.CIP_36,
                                                                                  "m/1852'/1815'/0'/2/0",
                                                                                  destinations["internalBaseWithStakingPath"],
                                                                                  1454448,
                                                                                  "m/1694'/1815'/101'/0/1"))),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a07582077be323b8df4c6aa1bf2f180112f85ffe8d7f658bc8febdf7dbd5a07453a31cb0807",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "328575c58f12eeee968f0ad3a10cfdba7551941cf1fd3238aa8003aadd2c2b59ebdae220e69e6f62392f38dc8a95343baf76772e5c9f10ca8e5805ae699a4a02")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with CIP36 registration with third-party payment address",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPath"]],
                               42,
                               10,
                               validityIntervalStart=7,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.CIP36_REGISTRATION,
                                                             TxAuxiliaryDataCIP36(CIP36VoteRegistrationFormat.CIP_36,
                                                                                  "m/1852'/1815'/0'/2/0",
                                                                                  destinations["externalShelleyBaseKeyhashScripthash"],
                                                                                  1454448,
                                                                                  "m/1694'/1815'/0'/0/1"))),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a07582042e408fb03986a958be9e2cca01623a31e23f86f31172a5a9b84acdfce6f0e750807",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "05836647a2864d20d0ddf1f591eecdaed02d54e60ecc40f9a881402c5bb9c401a10ae690dbca826242d1be0cd512c134e0bf03db7c81de90bf732e93fb36f501")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with CIP36 registration with voting purpose",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPath"]],
                               42,
                               10,
                               validityIntervalStart=7,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.CIP36_REGISTRATION,
                                                             TxAuxiliaryDataCIP36(CIP36VoteRegistrationFormat.CIP_36,
                                                                                  "m/1852'/1815'/0'/2/0",
                                                                                  destinations["internalBaseWithStakingPath"],
                                                                                  1454448,
                                                                                  "4b19e27ffc006ace16592311c4d2f0cafc255eaa47a6178ff540c0a46d07027c",
                                                                                  0))),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a075820d706aed1ebc1e8af188aae6d37ffdf4e259a0f04635bef5edce7f43ff632c4450807",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "2caf21060dfd08ec405df11578dc6380b0f375ceb74bc1c02e92d72310033d1c4309bd2488f3f6c4548e2f93099201663b59a2bded609d4abce2018a1d2b610c")])),
    SignTxTestCase("Sign tx with CIP36 registration with delegations",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPath"]],
                               42,
                               10,
                               validityIntervalStart=7,
                               auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.CIP36_REGISTRATION,
                                                             TxAuxiliaryDataCIP36(CIP36VoteRegistrationFormat.CIP_36,
                                                                                  "m/1852'/1815'/0'/2/0",
                                                                                  destinations["internalBaseWithStakingPath"],
                                                                                  1454448,
                                                                                  votingPurpose=2790,
                                                                                  delegations=[CIP36VoteDelegation(CIP36VoteDelegationType.KEY,
                                                                                                                   "4b19e27ffc006ace16592311c4d2f0cafc255eaa47a6178ff540c0a46d07027c",
                                                                                                                   9),
                                                                                               CIP36VoteDelegation(CIP36VoteDelegationType.PATH,
                                                                                                                   "m/1694'/1815'/0'/0/1",
                                                                                                                   0)]))),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a075820f0e62a047ef597d9fb1bfefb9cd3f4e77558c33510ca552484ee8b5c77bbdf650807",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "8a74428e0d7d74f94ff6e5cb7e1a2bec582225955d322e49b69a42ab47d2d768dc8b6aa9c50279f725af4d8579ba16d80606d858916452272353284d511e1806")]),
                   nano_skip=True),
]

# =================
# signTxPlutus
# =================
testsAlonzo = [
    SignTxTestCase("Sign tx with script data hash",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [],
                               42,
                               10,
                               scriptDataHash="ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce188",
                               includeNetworkId=True),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018002182a030a0b5820ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce1880f01",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "5c66a4f75359a62b4b32751fe30a1adbf7ed2839fd4cb762e9a4d2b086de82fca2310bcf07efc2b03086211faa19941dbe059bbfb747e128863f339720e71304")])),
    # tx does not contain any Plutus elements, but should be accepted (differs only in UI)
    SignTxTestCase("Sign tx with change output as array",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPath"]],
                               42,
                               10),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "ea26a98ce5399280ec8ad553d155c0900396204f9fe5a33969f279752a53263188d643544cdb4ffed108017bc7544e80df924143866638faffcd11646e57710b")])),
    SignTxTestCase("Sign tx with datum hash in output as array",
                   Transaction(Testnet,
                               [inputs["utxoShelley"]],
                               [outputs["datumHashExternal"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181835839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e1a006ca7935820ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce18802182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "afc57872d539c398acbb2d18c09796639029b4066ae3439925976d085b7150af418cf070b2ef80e907c20a2c942da4811b6847b1cd42fddc53d4c97732205d0d")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with datum hash in output as array with tokens",
                   Transaction(Testnet,
                               [inputs["utxoShelley"]],
                               [outputs["datumHashWithTokens"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181835839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e821a006ca793a1581c75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a2487564247542686911182f4875642475426869121a007838625820ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce18802182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "166a23e78036d5e776874bef45f86c757c60c5d1af83943982bbc1cc6bd68526cab1c554f2438c6a4c5491df00066b181891e5b97350e5b4fe367bf9a1317202")]),
                   nano_skip=True),
    # tests the path where a warning about missing datum hash is shown on Ledger
    SignTxTestCase("Sign tx with missing datum hash in output with tokens",
                   Transaction(Testnet,
                               [inputs["utxoShelley"]],
                               [outputs["missingDatumHashWithTokens"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e821a006ca793a1581c75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a2487564247542686911182f4875642475426869121a0078386202182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "ce096ed674ca863fb4af024f9341c8fd7fadd363ffc1b031cba65cb885f8d272759e69e44686e784d3a1e9b8b31c0e965752f13a79eb4095cd96ce26315c1903")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with collateral inputs",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [],
                               42,
                               10,
                               collateralInputs=[inputs["utxoShelley"]],
                               includeNetworkId=True),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018002182a030a0d818258201af8fa0b754ff99253d983894e63a2b09cbb56c833ba18c3384210163f63dcfc000f01",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "4867e65c60793b6bd60e677f30111d32f3f8dbf02a6f20985095bf8463b3062b5ad0669836d3e661dc1d0d710fd91f0756e6e5e0ab15cf829ab1f78226808a00"),
                                          Witness("m/44'/1815'/0'/0/1",
                                                  "be7162dc1348a79aa5260f33bda84c3eb5f909b108b444ff109bc8fa670fa032fe9951686e004f95453eaa49a73ee9f7c6193d215af804df1ac818ff31efbd01")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with required signers - mixed",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [],
                               42,
                               10,
                               requiredSigners=[RequiredSigner(TxRequiredSignerType.HASH, "fea6646c67fb467f8a5425e9c752e1e262b0420ba4b638f39514049a"),
                                                RequiredSigner(TxRequiredSignerType.PATH, "m/1852'/1815'/0'/0/0")],
                               includeNetworkId=True),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018002182a030a0e82581cfea6646c67fb467f8a5425e9c752e1e262b0420ba4b638f39514049a581c14c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11240f01",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "f5b2eb79b74678d3237b757dfcb8a623a8f7f5a10c5925b256da7723935bc98bbfc91ebc001d0e18c2929c611c99d43352ab33ee2dda45b6c115689ddaeeb502")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with mint path in a required signer",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10,
                               requiredSigners=[RequiredSigner(TxRequiredSignerType.PATH, "m/1855'/1815'/0'")]),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a0e81581c43040068ce85252be6164296d6dca9595644bbf424b56b7424458227",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "3dcd818effb503e4cf9d7c3836c29498d5258de7775915bf376eccae95e1b933afa5372478f136720b3c60346c9e674efea9f4b222916c96f0805962a16e9806"),
                                          Witness("m/1855'/1815'/0'",
                                                  "be7162dc1348a79aa5260f33bda84c3eb5f909b108b444ff109bc8f29d3410bf89fa938a73fb27df35a30910fb3111eb941e835946fd30c0bfcc377c7b8a8ac15dc807f995fb482efdf57e6d697d0d3effaa5cab104861698e39900a670fa032fe9951686e004f95453eaa49a73ee9f7c6193d215af804df1ac818ff31efbd01")]),
                   additionalWitnessPaths=["m/1855'/1815'/0'"]),
    SignTxTestCase("Sign tx with key hash in stake credential",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_DELEGATION,
                                                         StakeDelegationParams(CredentialParams(CredentialParamsType.KEY_HASH,
                                                                                                "29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd"),
                                                                               "f61c42cbf7c8c53af3f520508212ad3e72f674f957fe23ff0acb4973"))],
                               withdrawals=[Withdrawal(CredentialParams(CredentialParamsType.KEY_HASH,
                                                                        "29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd"),
                                                       1000)],
                               includeNetworkId=True),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a700818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018002182a030a048183028200581c29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd581cf61c42cbf7c8c53af3f520508212ad3e72f674f957fe23ff0acb497305a1581de129fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd1903e80f01",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "c986cf978bb08f49f0c50032c8eafa7fddce2a748d3bb0edc0245b5a205a60c55a5ad389d17b897cb83cfe34567c446afed4fd9d64a8304d02c55b9579685d0a")]),
                   nano_skip=True),
]

testsBabbage = [
    SignTxTestCase("Sign tx with short inline datum in output with tokens",
                   Transaction(Testnet_legacy,
                               [inputs["utxoShelley"]],
                               [outputs["inlineDatumWithTokensMap"]],
                               42,
                               10,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7"),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a3005839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e01821a006ca793a1581c75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a2487564247542686911182f4875642475426869121a00783862028201d818565579657420616e6f746865722063686f636f6c61746502182a030a0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "c2842b7a30a09634201425f707085b8eef73343ed69298e4e5d3887af362c8b92ee8f6d2c6a04bc7bb66ddcef35c27feb0efd046f5183a02c2267ebedb09780c")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with long inline datum (480 B) in output",
                   Transaction(Testnet_legacy,
                               [inputs["utxoShelley"]],
                               [outputs["inlineDatum480Map"]],
                               42,
                               10,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7"),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a3005839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e011a006ca793028201d8185901e012b8240c5470b47c159597b6f71d78c7fc99d1d8d911cb19b8f50211938ef361a22d30cd8f6354ec50e99a7d3cf3e06797ed4af3d358e01b2a957caa4010da328720b9fbe7a3a6d10209a13d2eb11933eb1bf2ab02713117e421b6dcc66297c41b95ad32d3457a0e6b44d8482385f311465964c3daff226acfb7bbda47011f1a6531db30e5b5977143c48f8b8eb739487f87dc13896f58529cfb48e415fc6123e708cdc3cb15cc1900ecf88c5fc9ff66d8ad6dae18c79e4a3c392a0df4d16ffa3e370f4dad8d8e9d171c5656bb317c78a2711057e7ae0beb1dc66ba01aa69d0c0db244e6742d7758ce8da00dfed6225d4aed4b01c42a0352688ed5803f3fd64873f11355305d9db309f4a2a6673cc408a06b8827a5edef7b0fd8742627fb8aa102a084b7db72fcb5c3d1bf437e2a936b738902a9c0258b462b9f2e9befd2c6bcfc036143bb34342b9124888a5b29fa5d60909c81319f034c11542b05ca3ff6c64c7642ff1e2b25fb60dc9bb6f5c914dd4149f31896955d4d204d822deddc46f852115a479edf7521cdf4ce596805875011855158fd303c33a2a7916a9cb7acaaf5aeca7e6efb75960e9597cd845bd9a93610bf1ab47ab0de943e8a96e26a24c4996f7b07fad437829fee5bc3496192608d4c04ac642cdec7bdbb8a948ad1d43402182a030a0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "9b45eae3e9e59f501adbe22ce7f22fbacce7c36623f28e1aa4fdb0942e58e839b02e21a6808a13c7490cbb70e9a174279b4c845dba3ee99b8d458cfa9d349908")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with long inline datum (304 B) in output with tokens",
                   Transaction(Testnet_legacy,
                               [inputs["utxoShelley"]],
                               [outputs["inlineDatum304WithTokensMap"]],
                               42,
                               10,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7"),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a3005839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e01821a006ca793a1581c75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a2487564247542686911182f4875642475426869121a00783862028201d8185901305579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f746865722063686f636f6c6174655579657420616e6f7468657220637468657202182a030a0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "e6baf473e8caabcdfaa961e4e25f31f1389de3528e6ffede36e8e23ac163a6b5fcab490f009577aa4f260a7f4e45d5b481f4b5c3542148feafcae101805f4001")]),
                   nano_skip=True),
    # reference script
    SignTxTestCase("Sign tx with datum hash and short ref. script in output",
                   Transaction(Testnet_legacy,
                               [inputs["utxoShelley"]],
                               [outputs["datumHashRefScriptExternalMap"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a4005839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e011a006ca7930282005820ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce18803d81854deadbeefdeadbeefdeadbeefdeadbeefdeadbeef02182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "09ac32c49b80265e668793ce441031b9bb8f99643ded6b3fa3f3c8109a287bd91a7fb899d137dd7333134ec748ee11a629aa252cfc9a75fd96217dfb08305003")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with datum hash and ref. script (240 B) in output in Babbage format",
                   Transaction(Testnet_legacy,
                               [inputs["utxoShelley"]],
                               [outputs["datumHashRefScript240ExternalMap"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a4005839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e011a006ca7930282005820ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce18803d81858f04784392787cc567ac21d7b5346a4a89ae112b7ff7610e402284042aa4e6efca7956a53c3f5cb3ec6745f5e21150f2a77bd71a2adc3f8b9539e9bab41934b477f60a8b302584d1a619ed9b178b5ce6fcad31adc0d6fc17023ede474c09f29fdbfb290a5b30b5240fae5de71168036201772c0d272ae90220181f9bf8c3198e79fc2ae32b076abf4d0e10d3166923ce56994b25c00909e3faab8ef1358c136cd3b197488efc883a7c6cfa3ac63ca9cebc62121c6e22f594420c2abd54e78282adec20ee7dba0e6de65554adb8ee8314f23f86cf7cf0906d4b6c643966baf6c54240c19f4131374e298f38a626a4ad63e6102182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "64d1ef5a3e8a074ad9ef34e0d6c19d313c09122f8cbbd54f3e46024b492e2d523a0ad1e132fc0fbf5ca4b2ddd2e72f110a9f669fef2f921a037553262aaffe06")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with datum hash and script reference (304 B) in output as map",
                   Transaction(Testnet_legacy,
                               [inputs["utxoShelley"]],
                               [outputs["datumHashRefScript304ExternalMap"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a4005839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e011a006ca7930282005820ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce18803d818590130deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeaddeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeaddeadbeef02182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "166e6a6749e103b0ddd2af822066a867fdf62f32fe91b6099a4d9983c6699f0d4da5d0be68afb5d41ecc54c2799665a6caf4beec8893c7f6593eaae3da8b0800")]),
                   nano_skip=True),
    # various output combinations
    SignTxTestCase("Sign tx with datum hash in output with tokens in Babbage format",
                   Transaction(Testnet_legacy,
                               [inputs["utxoShelley"]],
                               [outputs["datumHashWithTokensMap"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a400818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a3005839105e2f080eb93bad86d401545e0ce5f2221096d6477e11e6643922fa8d2ed495234dc0d667c1316ff84e572310e265edb31330448b36b7179e01821a006ca793a1581c75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a2487564247542686911182f4875642475426869121a007838620282005820ffd4d009f554ba4fd8ed1f1d703244819861a9d34fd4753bcf3ff32f043ce18802182a030a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "ec6d5db61abe1daa9d43ea0e4e89b9151227b3e5937cb304fa5d7823d555625327b19f71d890ddc73401e3dcad61903c32d889241643f64fb218f98828643f08")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with a complex multiasset output Babbage",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["multiassetManyTokensBabbage"],
                                outputs["internalBaseWithStakingPathBabbage"]],
                               42,
                               10,
                               validityIntervalStart=7),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000182a200583901eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff01821904d2a2581c7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373a34003581c1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df209015820000000000000000000000000000000000000000000000000000000000000000002581c95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a248456c204e69c3b16f1904d24874652474436f696e1a00783862a20058390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c011a006ca79302182a030a0807",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "6d31e18be58b2f5c8a2ea10e83d370418b51ef29e3f142f6605f3918d09fd78b5b520eb03332465d6304617b1a037cd4606e11f8ce4824038507d68bea5c6f02")]),
                   nano_skip=True),
    # reference inputs
    SignTxTestCase("Sign tx with change output as map and multiple reference inputs",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPathMap"]],
                               42,
                               10,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                               collateralInputs=[inputs["utxoShelley"]],
                               referenceInputs=[inputs["utxoShelley"], inputs["utxoShelley"]]),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a700818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a20058390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c011a006ca79302182a030a0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70d818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70012828258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7008258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "014537c021b33afaa6bd8909ce0059fceed55f6ee2db1e39b877dbdd3458d8ab9b1e632058916526ccf9b57f30a6f14006b3875ee400c59b5d43db3b0afd5b08")])),
    # total collateral and collateral return output
    SignTxTestCase("Sign tx with change output as map and total collateral",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPathMap"]],
                               42,
                               10,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                               totalCollateral=10),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a20058390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c011a006ca79302182a030a0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7110a",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "448576703ceed680504a957c12c973ec72d2562a49fbe978762668a5eb8fd767cb1b36a16018a573b23c1f669f35ec6401e73438f7414ae5f6e18ce304c71b0b")])),
    SignTxTestCase("Sign tx with change output as map and collateral output as array",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPathMap"]],
                               42,
                               10,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7"),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a20058390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c011a006ca79302182a030a0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7108258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca793",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "67615b4517feb87ed9a8a1b464de4e7e02264e02036538afc2091f8fef992c6b5de4e9b7f8a1cff7b21d25b6f71916161127119a63e076ce42d1e7289865d608")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with change collateral output as map without total collateral",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPathMap"]],
                               42,
                               10,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                               collateralInputs=[inputs["utxoShelley"]],
                               collateralOutput=outputs["internalBaseWithTokensMap"]),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a700818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a20058390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c011a006ca79302182a030a0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70d818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70010a20058390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c01821a006ca793a1581c75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a1487564247542686911182f",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "d17f32d31ba25f36b02b1b9994b9c3aea3df863a55dffc313612f2bf1f92e1fb65028cfd83f0b2f6b81c99ae2a02d2fc1e2d99b442e937fa33b5f4c0ccd6c60f")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with change collateral output as map with total collateral",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["internalBaseWithStakingPathMap"]],
                               42,
                               10,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                               collateralInputs=[inputs["utxoShelley"]],
                               collateralOutput=outputs["internalBaseWithTokensMap"],
                               totalCollateral=5),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a800818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a20058390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c011a006ca79302182a030a0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70d818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70010a20058390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c01821a006ca793a1581c75a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a1487564247542686911182f1105",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "c81d783cdf7b54ec560fce529169dd914b19a9ceb96371a5ec29c135c69e655a682c3e184216efd9557d53f6bcfd68c45031e3ad2c2e155a0bcff5095ab12608")]),
                   nano_skip=True),
    SignTxTestCase("Sign tx with third-party collateral output as map without total collateral",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["multiassetManyTokensBabbage"]],
                               42,
                               10,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                               collateralInputs=[inputs["utxoShelley"]],
                               collateralOutput=outputs["externalShelleyBaseKeyhashKeyhash"]),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a700818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a200583901eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff01821904d2a2581c7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373a34003581c1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df209015820000000000000000000000000000000000000000000000000000000000000000002581c95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a248456c204e69c3b16f1904d24874652474436f696e1a0078386202182a030a0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70d818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70010825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b0901",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "0d71240327d12b951d5953e7936dc87f91d10554aa7f476e82681d9584f95c50705ebf7d2f7f484ca552bbe385b4d09de1957d895a35e4f015c6a95bbb7c0707")])),
    SignTxTestCase("Sign tx with third-party collateral output as map with total collateral",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["multiassetManyTokensBabbage"]],
                               42,
                               10,
                               scriptDataHash="3b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7",
                               collateralInputs=[inputs["utxoShelley"]],
                               collateralOutput=outputs["externalShelleyBaseKeyhashKeyhash"],
                               totalCollateral=5),
                   TransactionSigningMode.PLUTUS_TRANSACTION,
                   "a800818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181a200583901eb0baa5e570cffbe2934db29df0b6a3d7c0430ee65d4c3a7ab2fefb91bc428e4720702ebd5dab4fb175324c192dc9bb76cc5da956e3c8dff01821904d2a2581c7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373a34003581c1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df209015820000000000000000000000000000000000000000000000000000000000000000002581c95a292ffee938be03e9bae5657982a74e9014eb4960108c9e23a5b39a248456c204e69c3b16f1904d24874652474436f696e1a0078386202182a030a0b58203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70d818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70010825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b09011105",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "f0048a529b21431fb4a4b39991b3301ff2c73994990da91b84484284f0681a8c0693e48b1c5bccd61c5c3533fdad2d89a481a85134b3a4e3c0805fdf05aa7f07")])),
]

# =================
# signTxPoolRegistration
# =================
poolRegistrationOwnerTestCases = [
     SignTxTestCase("Witness valid multiple mixed owners all relays pool registration",
                   Transaction(Mainnet,
                               [inputs["utxoNoPath"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationMixedOwnersAllRelays"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a04818a03581c13381d918ec0283ceeff60f7f4fc21e1540e053ccf8a77307a7a32ad582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad82581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c581c794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad848400190bb84436e44b9af68400190bb84436e44b9b500178ff2483e3a2330a34c4a5e576c2078301190bb86d616161612e626262622e636f6d82026d616161612e626262632e636f6d82782968747470733a2f2f7777772e76616375756d6c6162732e636f6d2f73616d706c6555726c2e6a736f6e5820cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/2/0",
                                                  "61fc06451462426b14fa3a31008a5f7d32b2f1793022060c02939bd0004b07f2bd737d542c2db6cef6dad912b9bdca1829a5dc2b45bab3c72afe374cef59cc04")]),
                   nano_skip=True),
     SignTxTestCase("Witness valid single path owner ipv4 relay pool registration",
                   Transaction(Mainnet,
                               [inputs["utxoNoPath"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationDefault"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a04818a03581c13381d918ec0283ceeff60f7f4fc21e1540e053ccf8a77307a7a32ad582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad81581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c818400190bb84436e44b9af682782968747470733a2f2f7777772e76616375756d6c6162732e636f6d2f73616d706c6555726c2e6a736f6e5820cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/2/0",
                                                  "f03947901bcfc96ac8e359825091db88900a470947c60220fcd3892683ec7fe949ef4e28a446d78a883f034cd77cbca669529a9da3f2316b762eb97033797a07")]),
                   nano_skip=True),
     SignTxTestCase("Witness valid multiple mixed owners ipv4 relay pool registration",
                   Transaction(Mainnet,
                               [inputs["utxoNoPath"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationMixedOwners"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a04818a03581c13381d918ec0283ceeff60f7f4fc21e1540e053ccf8a77307a7a32ad582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad82581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c581c794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad818400190bb84436e44b9af682782968747470733a2f2f7777772e76616375756d6c6162732e636f6d2f73616d706c6555726c2e6a736f6e5820cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/2/0",
                                                  "c1b454f3cf868007d2850084ff404bc4b91d9b541a78af9014288504143bd6b4f12df2163b7efb1817636eb625a62967fb66281ecae4d1b461770deafb65ba0f")]),
                   nano_skip=True),
     SignTxTestCase("Witness valid multiple mixed owners mixed ipv4, single host relays pool registration",
                   Transaction(Mainnet,
                               [inputs["utxoNoPath"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationMixedOwnersIpv4SingleHostRelays"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a04818a03581c13381d918ec0283ceeff60f7f4fc21e1540e053ccf8a77307a7a32ad582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad82581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c581c794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad828400190bb84436e44b9af68301190bb86d616161612e626262622e636f6d82782968747470733a2f2f7777772e76616375756d6c6162732e636f6d2f73616d706c6555726c2e6a736f6e5820cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/2/0",
                                                  "8bb8c10b390ac92f617ba6895e3b138f43dc741e3589a9548166d1eda995becf4a229e9e95f6300336f7e92345b244c5dc78cfe0cc12cac6ff6fbb5731671c0e")]),
                   nano_skip=True),
     SignTxTestCase("Witness valid multiple mixed owners mixed ipv4 ipv6 relays pool registration",
                   Transaction(Mainnet,
                               [inputs["utxoNoPath"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationMixedOwnersIpv4Ipv6Relays"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a04818a03581c13381d918ec0283ceeff60f7f4fc21e1540e053ccf8a77307a7a32ad582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad82581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c581c794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad828400190fa04436e44b9af68400190bb84436e44b9b500178ff2483e3a2330a34c4a5e576c20782782968747470733a2f2f7777772e76616375756d6c6162732e636f6d2f73616d706c6555726c2e6a736f6e5820cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/2/0",
                                                  "b0e6796ca5f97a0776c798e602afd0f6541996d431a3cbec8e3fe77eb49416cd812dcf6084672e40c9ae2b8cc8a5513d1b1a6c3ad408864d4a771e315c50d808")]),
                   nano_skip=True),
     SignTxTestCase("Witness valid single path owner no relays pool registration",
                   Transaction(Mainnet,
                               [inputs["utxoNoPath"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationNoRelays"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a04818a03581c13381d918ec0283ceeff60f7f4fc21e1540e053ccf8a77307a7a32ad582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad81581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c8082782968747470733a2f2f7777772e76616375756d6c6162732e636f6d2f73616d706c6555726c2e6a736f6e5820cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/2/0",
                                                  "adc06e34dc66f01b16496b04fc4ce5058e3be7290398cf2728f8463dda15c87866314449bdb309d0cdc22f3ca9bee310458f2769df6a1486f1b470a3227a030b")]),
                   nano_skip=True),
     SignTxTestCase("Witness pool registration with no metadata",
                   Transaction(Mainnet,
                               [inputs["utxoNoPath"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationNoMetadata"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a04818a03581c13381d918ec0283ceeff60f7f4fc21e1540e053ccf8a77307a7a32ad582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad81581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c818400190bb84436e44b9af6f6",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/2/0",
                                                  "06e66f6a2d510a8a5446597c59c79cbf4f9e7af9073da0651ea59bbdc2340dc933ed292aa282e6ea7068bed9f6bcb44228573e661c211e6dc61f4dd73ff41f04")]),
                   nano_skip=True),
     SignTxTestCase("Witness pool registration without outputs",
                   Transaction(Mainnet,
                               [inputs["utxoNoPath"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationMixedOwnersAllRelays"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018002182a030a04818a03581c13381d918ec0283ceeff60f7f4fc21e1540e053ccf8a77307a7a32ad582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad82581c1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c581c794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad848400190bb84436e44b9af68400190bb84436e44b9b500178ff2483e3a2330a34c4a5e576c2078301190bb86d616161612e626262622e636f6d82026d616161612e626262632e636f6d82782968747470733a2f2f7777772e76616375756d6c6162732e636f6d2f73616d706c6555726c2e6a736f6e5820cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/2/0",
                                                  "91c09ad95d5d0f87f61a62e2f5e2dda4245eb4011887a04a53bdf085282002ccc712718e855e36a30cfcf7ecd43bcdc795aa87647be9c716b65e7fcf376e0503")]),
                   nano_skip=True),
]

poolRegistrationOperatorTestCases = [
     SignTxTestCase("Witness pool registration as operator with no owners and no relays",
                   Transaction(Mainnet,
                               [inputs["utxoWithPath0"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationOperatorNoOwnersNoRelays"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OPERATOR,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a04818a03581cdbfee4665e58c8f8e9b9ff02b17f32e08a42c855476a5d867c2737b7582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad808082782968747470733a2f2f7777772e76616375756d6c6162732e636f6d2f73616d706c6555726c2e6a736f6e5820cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/2/0",
                                                  "2bff91cbd14ae53a2d476bd27306a7117d705c4fb58248af4f9b86c770991ea9785a39924d824a75b9ee0632b52c4267e6afec41e206a03b4753c5a397275807"),
                                          Witness("m/1853'/1815'/0'/0'",
                                                  "a92f621f48c785103b1dab862715beef0f0dc2408d0668422286a1dbc268db9a32cacd3b689a0c6af32ab2ac5057caac13910f09363e2d2db0dde4a27b2b5a09")]),
                   nano_skip=True),
     SignTxTestCase("Witness pool registration as operator with one owner and no relays",
                   Transaction(Mainnet,
                               [inputs["utxoWithPath0"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationOperatorOneOwnerOperatorNoRelays"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OPERATOR,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a04818a03581cdbfee4665e58c8f8e9b9ff02b17f32e08a42c855476a5d867c2737b7582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1eef1689a3970b7880dcf3cb4ca9f22453b3833824fea34105117c84081581c794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad8082782968747470733a2f2f7777772e76616375756d6c6162732e636f6d2f73616d706c6555726c2e6a736f6e5820cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "12776e69a6ea50ad42cdf0e164afc5a8b4fab612868ab990ead677ba4ced3ea2ad25b27ef5b27296add22c7378689a8572eb10ce24483b2ab8140b8aa5b1f70c"),
                                          Witness("m/1853'/1815'/0'/0'",
                                                  "d8851757cf6dc978fc4b3db42111124e83e99d58739a21ecf23c6b5de316a8fe6d03767df45e62ad7b64872a73a68427ce83f6a856ebd196897e4d96c3173d06")]),
                   nano_skip=True),
     SignTxTestCase("Witness pool registration as operator with multiple owners and all relays",
                   Transaction(Mainnet,
                               [inputs["utxoWithPath0"]],
                               [outputs["externalShelleyBaseKeyhashKeyhash"]],
                               42,
                               10,
                               certificates=[certificates["poolRegistrationOperatorMultipleOwnersAllRelays"]]),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OPERATOR,
                   "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b7000181825839017cb05fce110fb999f01abb4f62bc455e217d4a51fde909fa9aea545443ac53c046cf6a42095e3c60310fa802771d0672f8fe2d1861138b090102182a030a04818a03581cdbfee4665e58c8f8e9b9ff02b17f32e08a42c855476a5d867c2737b7582007821cd344d7fd7e3ae5f2ed863218cb979ff1d59e50c4276bdc479b0d0844501b0000000ba43b74001a1443fd00d81e82031864581de1794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad82581c794d9b3408c9fb67b950a48a0690f070f117e9978f7fc1d120fc58ad581c0bd5d796f5e54866a14300ec2a18d706f7461b8f0502cc2a182bc88d848400190bb84436e44b9af68400190bb84436e44b9b500178ff2483e3a2330a34c4a5e576c2078301190bb86d616161612e626262622e636f6d82026d616161612e626262632e636f6d82782968747470733a2f2f7777772e76616375756d6c6162732e636f6d2f73616d706c6555726c2e6a736f6e5820cdb714fd722c24aeb10c93dbb0ff03bd4783441cd5ba2a8b6f373390520535bb",
                   SignedTransactionData([Witness("m/1852'/1815'/0'/0/0",
                                                  "9f6da51173411ba82e76695ccf7c222f1df7444b0bbc1af354800acf244a4eaf72e95853406918e3ef461569fe99b39e33164ab440510f75df06e4ff89ca9107"),
                                          Witness("m/1853'/1815'/0'/0'",
                                                  "8957a7768bc9389cd7ab6fa3b3e2fa089785715a5298f9cb38abf99a6e0da5bef734c4862ca7948fb69575ccb9ed8ae1d92cc971742f674632f6f03e22c5b103")]),
                   nano_skip=True),
]

# =================
# Rejects
# =================
transactionInitRejectTestCases = [
     SignTxTestCase("Non-mainnet protocol magic",
                   Transaction(NetworkDesc(0x01, 764824072),
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "",
                   SignedTransactionData(sw=Errors.SW_REJECTED_BY_POLICY)),
     SignTxTestCase("Invalid network id",
                   Transaction(NetworkDesc(0x10, 764824073),
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "",
                   SignedTransactionData(sw=Errors.SW_INVALID_DATA)),
     SignTxTestCase("Pool registration (operator) - too few certificates",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OPERATOR,
                   "",
                   SignedTransactionData(sw=Errors.SW_REJECTED_BY_POLICY)),
     SignTxTestCase("Pool registration (owner) - too few certificates",
                   Transaction(Mainnet,
                               [inputs["utxoMultisig"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10),
                   TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
                   "",
                   SignedTransactionData(sw=Errors.SW_REJECTED_BY_POLICY)),
]

addressParamsRejectTestCases = [
     SignTxTestCase("Reward address - key",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [TxOutputAlonzo(destinations["paymentScriptPath"], 10)],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "",
                   SignedTransactionData(sw=Errors.SW_REJECTED_BY_POLICY)),
     SignTxTestCase("Reward address - script",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [TxOutputAlonzo(destinations["paymentScriptHash"], 10)],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "",
                   SignedTransactionData(sw=Errors.SW_REJECTED_BY_POLICY)),
]

# certificateRejectTestCases = [
# ]

certificateStakingRejectTestCases = [
     SignTxTestCase("Script hash in Ordinary Tx",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [TxOutputAlonzo(destinations["paymentScriptHash"], 10)],
                               42,
                               10,
                               certificates=[Certificate(CertificateType.STAKE_REGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.SCRIPT_HASH,
                                                                                                  "29fb5fd4aa8cadd6705acc8263cee0fc62edca5ac38db593fec2f9fd")))]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "",
                   SignedTransactionData(sw=Errors.SW_REJECTED_BY_POLICY)),
]

withdrawalRejectTestCases = [
     SignTxTestCase("Reject tx with invalid canonical ordering of withdrawals",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [],
                               42,
                               10,
                               withdrawals=[Withdrawal(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                        "m/1852'/1815'/0'/2/1"),
                                                       33333),
                                            Withdrawal(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                        "m/1852'/1815'/0'/2/0"),
                                                       33333)]),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "",
                   SignedTransactionData(sw=Errors.SW_INVALID_DATA)),
]

witnessRejectTestCases = [
     SignTxTestCase("Ordinary account path in Ordinary Tx",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["externalByronMainnet"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "",
                   SignedTransactionData(sw=Errors.SW_REJECTED_BY_POLICY),
                   additionalWitnessPaths=["m/1852'/1815'/0'"]),
]

testsInvalidTokenBundleOrdering = [
     SignTxTestCase("Reject tx where asset groups are not ordered",
                   Transaction(Mainnet,
                               [inputs["utxoShelley"]],
                               [outputs["multiassetInvalidAssetGroupOrdering"]],
                               42,
                               10),
                   TransactionSigningMode.ORDINARY_TRANSACTION,
                   "",
                   SignedTransactionData(sw=Errors.SW_INVALID_DATA)),
]

singleAccountRejectTestCases = [
        SignTxTestCase("Input and change output account mismatch",
                    Transaction(Mainnet,
                                [inputs["utxoShelley"]],
                                [TxOutputBabbage(destinations["multiassetThirdParty"], 1)],
                                42,
                                10,
                               certificates=[Certificate(CertificateType.STAKE_DEREGISTRATION,
                                                         StakeRegistrationParams(CredentialParams(CredentialParamsType.KEY_PATH,
                                                                                                  "m/1852'/1815'/1'/2/0")))]),
                    TransactionSigningMode.ORDINARY_TRANSACTION,
                    "",
                    SignedTransactionData(sw=Errors.SW_REJECTED_BY_POLICY)),
]

collateralOutputRejectTestCases = [
        SignTxTestCase("Collateral output with datum hash",
                    Transaction(Mainnet,
                                [inputs["utxoShelley"]],
                                [outputs["externalByronMainnet"]],
                                42,
                                10,
                                collateralOutput=outputs["datumHashExternal"]),
                    TransactionSigningMode.PLUTUS_TRANSACTION,
                    "",
                    SignedTransactionData(sw=Errors.SW_REJECTED_BY_POLICY)),
]

testsCVoteRegistrationRejects = [
        SignTxTestCase("CIP15 registration with delegation",
                    Transaction(Mainnet,
                                [inputs["utxoShelley"]],
                                [outputs["externalByronMainnet"]],
                                42,
                                10,
                                auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.CIP36_REGISTRATION,
                                                              TxAuxiliaryDataCIP36(CIP36VoteRegistrationFormat.CIP_15,
                                                                                   "m/1852'/1815'/0'/2/0",
                                                                                   destinations["internalBaseWithStakingPath"],
                                                                                   1454448,
                                                                                   "4b19e27ffc006ace16592311c4d2f0cafc255eaa47a6178ff540c0a46d07027c",
                                                                                   delegations=[CIP36VoteDelegation(CIP36VoteDelegationType.KEY,
                                                                                                                    "4b19e27ffc006ace16592311c4d2f0cafc255eaa47a6178ff540c0a46d07027c",
                                                                                                                    0)]))),
                    TransactionSigningMode.ORDINARY_TRANSACTION,
                    "",
                    SignedTransactionData(sw=Errors.SW_INVALID_DATA)),
        SignTxTestCase("CIP15 registration with voting purpose",
                    Transaction(Mainnet,
                                [inputs["utxoShelley"]],
                                [outputs["externalByronMainnet"]],
                                42,
                                10,
                                auxiliaryData=TxAuxiliaryData(TxAuxiliaryDataType.CIP36_REGISTRATION,
                                                              TxAuxiliaryDataCIP36(CIP36VoteRegistrationFormat.CIP_15,
                                                                                   "m/1852'/1815'/0'/2/0",
                                                                                   destinations["internalBaseWithStakingPath"],
                                                                                   1454448,
                                                                                   "4b19e27ffc006ace16592311c4d2f0cafc255eaa47a6178ff540c0a46d07027c",
                                                                                   votingPurpose=0))),
                    TransactionSigningMode.ORDINARY_TRANSACTION,
                    "",
                    SignedTransactionData(sw=Errors.SW_INVALID_DATA)),
]
