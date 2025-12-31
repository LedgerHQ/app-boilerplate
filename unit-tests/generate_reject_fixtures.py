#!/usr/bin/env python3
"""
Regenerate the reject test fixtures from LedgerJS data so unit-tests/test_sign_tx_rejects.c
stays in sync with the JavaScript fixtures.

The script calls a small Node helper that exports the reject fixture definitions, converts
them into the Python transaction structures already used by the command builder, and then
produces C-friendly arrays with the init payloads and data chunks plus metadata.

Usage:
  python3 generate_reject_fixtures.py

The generated file replaces the handwritten fixture table in unit-tests/test_sign_tx_rejects.c.
"""

from __future__ import annotations

import json
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Type

REPO_ROOT = Path(__file__).resolve().parent.parent
NODE_SCRIPT = REPO_ROOT / "unit-tests" / "export_sign_tx_rejects.js"
MOCK_USB_LOADER = REPO_ROOT / "unit-tests" / "mock_usb_loader.js"
NODE_VERSION = "16.20.2"

NODE_CMD = (
    "bash",
    "-lc",
    f"source ~/.nvm/nvm.sh && nvm use {NODE_VERSION} >/dev/null && "
    f"cd {REPO_ROOT / '..' / 'ledgerjs-cardano-shelley'} && "
    f"NODE_OPTIONS=--require={MOCK_USB_LOADER} NODE_PATH=./node_modules "
    f"node {NODE_SCRIPT}",
)

sys.path.insert(0, str(REPO_ROOT / "tests"))
sys.path.insert(0, str(REPO_ROOT / "tests" / "application_client"))
sys.path.insert(0, str(REPO_ROOT / "tests" / "standalone"))

from application_client.app_def import AddressType, NetworkDesc
from application_client.command_builder import CommandBuilder, P1Type, gather_witness_paths
from standalone.input_files.derive_address import DeriveAddressTestCase, pointer_to_str
from standalone.input_files.signTx import (
    AnchorParams,
    AssetGroup,
    Certificate,
    CertificateType,
    CredentialParams,
    CredentialParamsType,
    Datum,
    DatumType,
    Margin,
    MultiHostRelayParams,
    PoolKey,
    PoolKeyType,
    PoolMetadataParams,
    PoolRegistrationParams,
    PoolRetirementParams,
    Relay,
    RelayType,
    SingleHostHostnameRelayParams,
    SingleHostIpAddrRelayParams,
    StakeDelegationParams,
    StakeRegistrationParams,
    Token,
    Transaction,
    TransactionSigningMode,
    TxAuxiliaryData,
    TxAuxiliaryDataHash,
    TxAuxiliaryDataType,
    TxInput,
    TxOutputAlonzo,
    TxOutputBabbage,
    TxOutputDestination,
    TxOutputDestinationType,
    TxOutputFormat,
    TxRequiredSignerType,
    RequiredSigner,
    Withdrawal,
    Voter,
    VoterType,
    Vote,
    VoteOption,
    VoterVotes,
    VotingProcedure,
    GovActionId,
    ThirdPartyAddressParams,
)

GENERATED_HEADER = REPO_ROOT / "unit-tests" / "generated_sign_tx_rejects.h"
C_SOURCE_FILE = REPO_ROOT / "unit-tests" / "test_sign_tx_rejects.c"

SET_ORDER = [
    "transactionInitRejectTestCases",
    "addressParamsRejectTestCases",
    "certificateRejectTestCases",
    "certificateStakingRejectTestCases",
    "certificateStakePoolRetirementRejectTestCases",
    "withdrawalRejectTestCases",
    "witnessRejectTestCases",
    "singleAccountRejectTestCases",
    "collateralOutputRejectTestCases",
    "testsInvalidTokenBundleOrdering",
    "poolRegistrationOwnerRejectTestCases",
    "stakePoolRegistrationPoolIdRejectTestCases",
    "stakePoolRegistrationOwnerRejectTestCases",
    "outputRejectTestCases",
]

SET_PREFIX = {
    "transactionInitRejectTestCases": "REJECT_INIT",
    "addressParamsRejectTestCases": "REJECT_ADDRESS",
    "certificateRejectTestCases": "REJECT_CERT",
    "certificateStakingRejectTestCases": "REJECT_CERT_STAKING",
    "certificateStakePoolRetirementRejectTestCases": "REJECT_CERT_POOL_RETIRE",
    "withdrawalRejectTestCases": "REJECT_WITHDRAWAL",
    "witnessRejectTestCases": "REJECT_WITNESS",
    "singleAccountRejectTestCases": "REJECT_SINGLE_ACCOUNT",
    "collateralOutputRejectTestCases": "REJECT_COLLATERAL_OUTPUT",
    "testsInvalidTokenBundleOrdering": "REJECT_MULTIASSET",
    "poolRegistrationOwnerRejectTestCases": "REJECT_POOL_OWNER",
    "stakePoolRegistrationPoolIdRejectTestCases": "REJECT_POOL_ID",
    "stakePoolRegistrationOwnerRejectTestCases": "REJECT_POOL_OWNER",
    "outputRejectTestCases": "REJECT_OUTPUT",
}

TSIGNING_MODE_MAP = {
    "ordinary_transaction": TransactionSigningMode.ORDINARY_TRANSACTION,
    "pool_registration_as_owner": TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
    "pool_registration_as_operator": TransactionSigningMode.POOL_REGISTRATION_AS_OPERATOR,
    "multisig_transaction": TransactionSigningMode.MULTISIG_TRANSACTION,
    "plutus_transaction": TransactionSigningMode.PLUTUS_TRANSACTION,
}

REJECT_REASON_SW: Dict[str, str] = {
    "InvalidDataReason.NETWORK_INVALID_NETWORK_ID": "SWO_INVALID_NETWORK_ID",
    "InvalidDataReason.NETWORK_INVALID_PROTOCOL_MAGIC": "SWO_INVALID_PROTOCOL_MAGIC",
    "InvalidDataReason.MULTIASSET_INVALID_TOKEN_BUNDLE_ORDERING": "SWO_TX_PARSING_FAIL_CANONICAL_ORDER",
    "InvalidDataReason.MULTIASSET_INVALID_TOKEN_BUNDLE_NOT_UNIQUE": "SWO_TX_PARSING_FAIL_CANONICAL_ORDER",
    "InvalidDataReason.MULTIASSET_INVALID_ASSET_GROUP_ORDERING": "SWO_TX_PARSING_FAIL_CANONICAL_ORDER",
    "InvalidDataReason.MULTIASSET_INVALID_ASSET_GROUP_NOT_UNIQUE": "SWO_TX_PARSING_FAIL_CANONICAL_ORDER",
    "InvalidDataReason.SIGN_MODE_ORDINARY__POOL_REGISTRATION_NOT_ALLOWED": "SWO_TX_PARSING_FAIL_CERTIFICATES",
    "InvalidDataReason.SIGN_MODE_MULTISIG__POOL_REGISTRATION_NOT_ALLOWED": "SWO_TX_PARSING_FAIL_CERTIFICATES",
    "InvalidDataReason.SIGN_MODE_PLUTUS__POOL_REGISTRATION_NOT_ALLOWED": "SWO_TX_PARSING_FAIL_CERTIFICATES",
    "InvalidDataReason.SIGN_MODE_POOL_OPERATOR__SINGLE_POOL_REG_CERTIFICATE_REQUIRED": "SWO_TX_PARSING_FAIL_CERTIFICATES",
    "InvalidDataReason.SIGN_MODE_POOL_OWNER__SINGLE_POOL_REG_CERTIFICATE_REQUIRED": "SWO_TX_PARSING_FAIL_CERTIFICATES",
    "InvalidDataReason.CERTIFICATE_INVALID_POOL_KEY_HASH": "SWO_TX_PARSING_FAIL_CERTIFICATES",
}

def parse_enum(enum_cls: Type[Any], value: Any):
    if isinstance(value, enum_cls):
        return value
    if isinstance(value, str):
        normalized = value.replace("-", "_").replace(" ", "_").upper()
        members = enum_cls.__members__
        if normalized in members:
            return enum_cls[normalized]
        suffix = normalized.split("_")[-1]
        if suffix in members:
            return enum_cls[suffix]
        raise KeyError(f"{normalized} not found in {enum_cls.__name__}")
    return enum_cls(value)


CREDENTIAL_PARAMS_TYPE_MAP: Dict[int, CredentialParamsType] = {
    0: CredentialParamsType.KEY_PATH,
    1: CredentialParamsType.KEY_HASH,
    2: CredentialParamsType.SCRIPT_HASH,
}


def parse_credential_params_type(value: Any) -> CredentialParamsType:
    if isinstance(value, CredentialParamsType):
        return value
    if isinstance(value, str):
        return parse_enum(CredentialParamsType, value)
    if isinstance(value, int):
        mapped = CREDENTIAL_PARAMS_TYPE_MAP.get(value)
        if mapped is not None:
            return mapped
    raise ValueError(f"Unknown CredentialParamsType value: {value}")

def run_node_export() -> Dict[str, Any]:
    proc = subprocess.run(NODE_CMD, capture_output=True, text=True, check=True)
    return json.loads(proc.stdout)


def to_bip32_path(path: Sequence[int]) -> str:
    HARDENED = 0x80000000
    components: List[str] = []
    for segment in path:
        if segment >= HARDENED:
            components.append(f"{segment - HARDENED}'")
        else:
            components.append(str(segment))
    return "m/" + "/".join(components)


def convert_network(network_json: Dict[str, Any]) -> NetworkDesc:
    return NetworkDesc(networkId=network_json["networkId"], protocol=network_json["protocolMagic"])


def convert_input(input_json: Dict[str, Any]) -> TxInput:
    path = input_json.get("path")
    return TxInput(
        txHashHex=input_json["txHashHex"],
        path=to_bip32_path(path) if path else None,
        outputIndex=int(input_json.get("outputIndex", 0)),
    )


def convert_credential(credential: Dict[str, Any]) -> CredentialParams:
    ctype = parse_credential_params_type(credential["type"])
    if ctype == CredentialParamsType.KEY_PATH:
        path_value = credential.get("keyPath") or credential.get("path")
        if path_value is None:
            raise ValueError("Missing keyPath for KEY_PATH credential")
        return CredentialParams(type=ctype, keyValue=to_bip32_path(path_value))
    if ctype == CredentialParamsType.KEY_HASH:
        hash_value = credential.get("keyHashHex") or credential.get("hashHex")
        if hash_value is None:
            raise ValueError("Missing keyHashHex for KEY_HASH credential")
        return CredentialParams(type=ctype, keyValue=hash_value.lower())
    if ctype == CredentialParamsType.SCRIPT_HASH:
        script_hash = credential.get("scriptHashHex")
        if script_hash is None:
            raise ValueError("Missing scriptHashHex for SCRIPT_HASH credential")
        return CredentialParams(type=ctype, keyValue=script_hash.lower())
    raise ValueError(f"Unsupported credential type: {credential}")


def convert_asset_group(group: Dict[str, Any]) -> AssetGroup:
    tokens = [
        Token(assetNameHex=token["assetNameHex"].lower(), amount=int(token["amount"]))
        for token in group["tokens"]
    ]
    return AssetGroup(policyIdHex=group["policyIdHex"].lower(), tokens=tokens)


def convert_datum(datum_json: Dict[str, Any]) -> Datum:
    datum_type = parse_enum(DatumType, datum_json["type"])
    if datum_type == DatumType.HASH:
        return Datum(type=datum_type, datumHex=datum_json["datumHashHex"].lower())
    return Datum(type=datum_type, datumHex=datum_json["datumHex"].lower())


def convert_pool_key(pool_key_json: Dict[str, Any]) -> PoolKey:
    key_type = parse_enum(PoolKeyType, pool_key_json["type"])
    params = pool_key_json["params"]
    if key_type == PoolKeyType.DEVICE_OWNED:
        path_value = params.get("path") or params.get("spendingPath") or params.get("stakingPath")
        if path_value is None:
            raise ValueError(f"Missing path for device owned pool key: {pool_key_json}")
        return PoolKey(type=key_type, key=to_bip32_path(path_value))
    hash_value = (
        params.get("keyHashHex")
        or params.get("rewardAccountHex")
        or params.get("stakingKeyHashHex")
        or params.get("stakingScriptHashHex")
    )
    if not hash_value:
        raise ValueError(f"Missing hash for third-party pool key: {pool_key_json}")
    return PoolKey(type=key_type, key=hash_value.lower())


def convert_relay(relay_json: Dict[str, Any]) -> Relay:
    relay_type = parse_enum(RelayType, relay_json["type"])
    params = relay_json["params"]
    if relay_type == RelayType.SINGLE_HOST_IP_ADDR:
        return Relay(
            type=relay_type,
            params=SingleHostIpAddrRelayParams(
                portNumber=params.get("portNumber"),
                ipv4=params.get("ipv4"),
                ipv6=params.get("ipv6"),
            ),
        )
    if relay_type == RelayType.SINGLE_HOST_HOSTNAME:
        return Relay(
            type=relay_type,
            params=SingleHostHostnameRelayParams(portNumber=params["portNumber"], dnsName=params["dnsName"]),
        )
    return Relay(type=relay_type, params=MultiHostRelayParams(dnsName=params["dnsName"]))


def convert_pool_registration_params(params_json: Dict[str, Any]) -> PoolRegistrationParams:
    margin_json = params_json["margin"]
    metadata_json = params_json.get("metadata")
    return PoolRegistrationParams(
        poolKey=convert_pool_key(params_json["poolKey"]),
        vrfKeyHashHex=params_json["vrfKeyHashHex"].lower(),
        pledge=int(params_json["pledge"]),
        cost=int(params_json["cost"]),
        margin=Margin(numerator=int(margin_json["numerator"]), denominator=int(margin_json["denominator"])),
        rewardAccount=convert_pool_key(params_json["rewardAccount"]),
        poolOwners=[convert_pool_key(owner) for owner in params_json["poolOwners"]],
        relays=[convert_relay(relay) for relay in params_json["relays"]],
        metadata=PoolMetadataParams(
            metadata_json["metadataUrl"], metadata_json["metadataHashHex"].lower()
        )
        if metadata_json
        else None,
    )


def convert_certificate(cert_json: Dict[str, Any]) -> Certificate:
    cert_type = parse_enum(CertificateType, cert_json["type"])
    params = cert_json["params"]
    if cert_type == CertificateType.STAKE_REGISTRATION:
        return Certificate(type=cert_type, params=StakeRegistrationParams(stakeCredential=convert_credential(params["stakeCredential"])))
    if cert_type == CertificateType.STAKE_DEREGISTRATION:
        return Certificate(type=cert_type, params=StakeRegistrationParams(stakeCredential=convert_credential(params["stakeCredential"])))
    if cert_type == CertificateType.STAKE_DELEGATION:
        return Certificate(
            type=cert_type,
            params=StakeDelegationParams(
                stakeCredential=convert_credential(params["stakeCredential"]),
                poolKeyHash=params["poolKeyHashHex"].lower(),
            ),
        )
    if cert_type == CertificateType.STAKE_POOL_REGISTRATION:
        return Certificate(type=cert_type, params=convert_pool_registration_params(params))
    if cert_type == CertificateType.STAKE_POOL_RETIREMENT:
        pool_key_path = params.get("poolKeyPath")
        if pool_key_path is None:
            pool_key_path = params.get("poolCredentialPath")
        if pool_key_path is None:
            raise ValueError("Missing poolKeyPath for pool retirement certificate")
        pool_credential = CredentialParams(
            type=CredentialParamsType.KEY_PATH,
            keyValue=to_bip32_path(pool_key_path),
        )
        return Certificate(
            type=cert_type,
            params=PoolRetirementParams(
                poolCredential=pool_credential,
                retirementEpoch=int(params["retirementEpoch"]),
            ),
        )
    raise ValueError(f"Unsupported certificate type: {cert_type}")


def convert_withdrawal(withdraw_json: Dict[str, Any]) -> Withdrawal:
    return Withdrawal(
        stakeCredential=convert_credential(withdraw_json["stakeCredential"]),
        amount=int(withdraw_json["amount"]),
    )


def convert_required_signers(signers_json: Sequence[Dict[str, Any]]) -> List[RequiredSigner]:
    result: List[RequiredSigner] = []
    for signer in signers_json:
        signer_type = parse_enum(TxRequiredSignerType, signer["type"])
        if signer_type == TxRequiredSignerType.PATH:
            result.append(RequiredSigner(type=signer_type, pathOrHashHex=to_bip32_path(signer["path"])))
        else:
            result.append(RequiredSigner(type=signer_type, pathOrHashHex=signer["hashHex"].lower()))
    return result


def convert_vote(vote_json: Dict[str, Any]) -> Vote:
    gov_action = vote_json["govActionId"]
    return Vote(
        govActionId=GovActionId(txHashHex=gov_action["txHashHex"], govActionIndex=int(gov_action["govActionIndex"])),
        votingProcedure=VotingProcedure(
            vote=parse_enum(VoteOption, vote_json["votingProcedure"]["vote"]),
            anchor=AnchorParams(
                url=vote_json["votingProcedure"]["anchor"]["url"],
                hashHex=vote_json["votingProcedure"]["anchor"]["hashHex"].lower(),
            )
            if vote_json["votingProcedure"].get("anchor")
            else None,
        ),
    )


def convert_voter(voter_json: Dict[str, Any]) -> Voter:
    return Voter(type=parse_enum(VoterType, voter_json["type"]), keyValue=voter_json["keyValue"])


def convert_voting_procedures(procedures_json: Sequence[Dict[str, Any]]) -> List[VoterVotes]:
    result: List[VoterVotes] = []
    for entry in procedures_json:
        result.append(
            VoterVotes(
                voter=convert_voter(entry["voter"]),
                votes=[convert_vote(v) for v in entry.get("votes", [])],
            )
        )
    return result


def convert_device_owned_destination(network: NetworkDesc, dest_json: Dict[str, Any]) -> TxOutputDestination:
    params = dest_json["params"]["params"]
    addr_type = parse_enum(AddressType, dest_json["params"]["type"])
    spending_value = ""
    if "spendingPath" in params:
        spending_value = to_bip32_path(params["spendingPath"])
    elif "spendingScriptHashHex" in params:
        spending_value = params["spendingScriptHashHex"].lower()

    staking_value = ""
    if "stakingPath" in params:
        staking_value = to_bip32_path(params["stakingPath"])
    elif "stakingKeyHashHex" in params:
        staking_value = params["stakingKeyHashHex"].lower()
    elif "stakingScriptHashHex" in params:
        staking_value = params["stakingScriptHashHex"].lower()
    elif "stakingBlockchainPointer" in params:
        pointer = params["stakingBlockchainPointer"]
        staking_value = pointer_to_str(pointer["blockIndex"], pointer["txIndex"], pointer["certificateIndex"])

    return TxOutputDestination(
        TxOutputDestinationType.DEVICE_OWNED,
        DeriveAddressTestCase("", network, addr_type, spending_value, staking_value),
    )


def convert_destination(network: NetworkDesc, dest_json: Dict[str, Any]) -> TxOutputDestination:
    if dest_json["type"] == "third_party":
        return TxOutputDestination(
            TxOutputDestinationType.THIRD_PARTY,
            ThirdPartyAddressParams(dest_json["params"]["addressHex"].lower()),
        )
    return convert_device_owned_destination(network, dest_json)


def convert_output(network: NetworkDesc, output_json: Dict[str, Any]) -> Any:
    destination = convert_destination(network, output_json["destination"])
    amount = int(output_json["amount"])
    token_bundle = [convert_asset_group(group) for group in output_json.get("tokenBundle", [])]
    datum = None
    if output_json.get("datum"):
        datum = convert_datum(output_json["datum"])
    elif output_json.get("datumHashHex"):
        datum = Datum(type=DatumType.HASH, datumHex=output_json["datumHashHex"].lower())

    reference_script = output_json.get("referenceScriptHex")
    if reference_script:
        reference_script = reference_script.lower()

    format_raw = output_json.get("format")
    format_value = parse_enum(TxOutputFormat, format_raw) if format_raw is not None else TxOutputFormat.ARRAY_LEGACY

    if format_value == TxOutputFormat.MAP_BABBAGE:
        return TxOutputBabbage(
            destination,
            amount,
            format=format_value,
            tokenBundle=token_bundle,
            datum=datum,
            referenceScriptHex=reference_script,
        )
    return TxOutputAlonzo(
        destination,
        amount,
        format=format_value,
        tokenBundle=token_bundle,
        datum=datum,
    )


def convert_auxiliary_data(data: Optional[Dict[str, Any]]) -> Optional[TxAuxiliaryData]:
    if data is None:
        return None
    aux_type = parse_enum(TxAuxiliaryDataType, data["type"])
    if aux_type == TxAuxiliaryDataType.ARBITRARY_HASH:
        return TxAuxiliaryData(
            type=TxAuxiliaryDataType.ARBITRARY_HASH,
            params=TxAuxiliaryDataHash(data["params"]["hashHex"].lower()),
        )
    raise ValueError("Unsupported auxiliary data type")


def convert_transaction(tx_json: Dict[str, Any]) -> Transaction:
    network = convert_network(tx_json["network"])
    tx = Transaction(
        network=network,
        inputs=[convert_input(inp) for inp in tx_json.get("inputs", [])],
        outputs=[convert_output(network, out) for out in tx_json.get("outputs", [])],
        fee=int(tx_json.get("fee", 0)),
        ttl=tx_json.get("ttl"),
        certificates=[convert_certificate(cert) for cert in tx_json.get("certificates", [])],
        withdrawals=[convert_withdrawal(w) for w in tx_json.get("withdrawals", [])],
        mint=[convert_asset_group(group) for group in tx_json.get("mint", [])],
        collateralInputs=[convert_input(inp) for inp in tx_json.get("collateralInputs", [])],
        requiredSigners=convert_required_signers(tx_json.get("requiredSigners", [])),
        referenceInputs=[convert_input(inp) for inp in tx_json.get("referenceInputs", [])],
        votingProcedures=convert_voting_procedures(tx_json.get("votingProcedures", [])),
        auxiliaryData=convert_auxiliary_data(tx_json.get("auxiliaryData")),
        validityIntervalStart=tx_json.get("validityIntervalStart"),
        scriptDataHash=tx_json.get("scriptDataHashHex"),
        includeNetworkId=tx_json.get("includeNetworkId"),
        collateralOutput=convert_output(network, tx_json["collateralOutput"])
        if tx_json.get("collateralOutput")
        else None,
        totalCollateral=tx_json.get("totalCollateral"),
        treasury=tx_json.get("treasury"),
        donation=tx_json.get("donation"),
    )
    return tx


def sanitize_name(name: str) -> str:
    result = []
    for char in name.replace("-", "_"):
        if char.isalnum():
            result.append(char.upper())
        elif char == "_":
            result.append("_")
        else:
            result.append("_")
    cleaned = "_".join(part for part in "".join(result).split("_") if part)
    return cleaned


def format_display_name(prefix: str, test_name: str, reason: Optional[str] = None) -> str:
    cleaned = test_name.replace("-", "").replace(" ", "_")
    cleaned = "_".join(part for part in cleaned.split("_") if part)
    if reason:
        reason_label = reason.split(".")[-1]
        reason_label = sanitize_name(reason_label)
        if reason_label:
            cleaned = f"{cleaned}_{reason_label}"
    return f"[{prefix}] {cleaned}"


def to_hex_lines(hex_str: str, indent: int = 4, append_comma: bool = False) -> List[str]:
    chunk_size = 64
    lines = []
    for i in range(0, len(hex_str), chunk_size):
        segment = hex_str[i : i + chunk_size]
        lines.append(" " * indent + f"\"{segment}\"")
    if append_comma and lines:
        lines[-1] = lines[-1] + ","
    return lines


def reject_reason_to_status_word(prefix: str,
                                 reason: Optional[str],
                                 fixture_name: str,
                                 tx: Transaction) -> str:
    if fixture_name == "Non-mainnet protocol magic":
        return "SWO_INVALID_PROTOCOL_MAGIC"
    if fixture_name == "Invalid network id":
        return "SWO_INVALID_NETWORK_ID"
    if prefix == "REJECT_INIT":
        return "SWO_SECURITY_CONDITION_NOT_SATISFIED"
    if reason and reason in REJECT_REASON_SW:
        return REJECT_REASON_SW[reason]
    return "SWO_SECURITY_CONDITION_NOT_SATISFIED"


@dataclass
class ChunkInfo:
    p1: int
    more: bool
    hex_payload: str


@dataclass
class FixtureInfo:
    name: str
    display_name: str
    prefix: str
    sanitized_name: str
    init_hex: str
    chunks: List[ChunkInfo]
    expected_sw: str
    expect_init_failure: bool
    reject_reason: Optional[str]


def build_fixture(fixture_json: Dict[str, Any], prefix: str) -> FixtureInfo:
    tx = convert_transaction(fixture_json["tx"])
    builder = CommandBuilder()
    witness_paths = gather_witness_paths(tx, fixture_json.get("additionalWitnessPaths", []))
    init_params = builder.build_tx_init_params(
        tx=tx,
        signing_mode=TSIGNING_MODE_MAP[fixture_json["signingMode"]],
        witness_paths=witness_paths,
    )
    init_payload = builder.sign_tx_init(init_params)[5:]
    chunks = [
        ChunkInfo(
            p1=chunk[2],
            more=chunk[2] == P1Type.P1_TX_DATA_CHUNK,
            hex_payload=chunk[5:].hex().upper(),
        )
        for chunk in builder.serialize_transaction_chunks(tx)
    ]
    reason = fixture_json.get("rejectReason")
    expect_init_failure = prefix == "REJECT_INIT"
    display_name = format_display_name(prefix, fixture_json["testName"], reason)
    return FixtureInfo(
        name=fixture_json["testName"],
        display_name=display_name,
        prefix=prefix,
        sanitized_name=sanitize_name(fixture_json["testName"]),
        init_hex=init_payload.hex().upper(),
        chunks=chunks,
        expected_sw=reject_reason_to_status_word(prefix, reason, fixture_json["testName"], tx),
        expect_init_failure=expect_init_failure,
        reject_reason=reason,
    )


def generate_header(fixtures: Dict[str, List[FixtureInfo]]) -> str:
    lines = [
        "// Auto-generated file. Do not edit directly.",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "#include <stdbool.h>",
        "",
    ]
    for set_name in SET_ORDER:
        prefix = SET_PREFIX.get(set_name)
        if not prefix or set_name not in fixtures:
            continue
        for fixture in fixtures[set_name]:
            if not fixture.chunks:
                continue
            if fixture.reject_reason:
                lines.append(f"// {fixture.reject_reason}")
            lines.append(f"static const apdu_segment_t SIGN_TX_SEGMENTS_{prefix}_{fixture.sanitized_name}[] = {{")
            for chunk in fixture.chunks:
                lines.append("    {")
                lines.append("        .hex_payload =")
                lines.extend(to_hex_lines(chunk.hex_payload, append_comma=True))
                lines.append(f"        .p1 = 0x{chunk.p1:02X},")
                lines.append(f"        .more = {'true' if chunk.more else 'false'},")
                lines.append("    },")
            lines.append("};")
            lines.append("")
        lines.append("")
    lines.append("static const sign_tx_reject_fixture_t SIGN_TX_REJECT_FIXTURES[] = {")
    for set_name in SET_ORDER:
        prefix = SET_PREFIX.get(set_name)
        if not prefix or set_name not in fixtures:
            continue
        for fixture in fixtures[set_name]:
            if fixture.reject_reason:
                lines.append(f"    // {fixture.reject_reason}")
            lines.append("    {")
            lines.append(f'        .name = "{fixture.display_name}",')
            lines.append(f'        .init_hex =')
            lines.extend(to_hex_lines(fixture.init_hex, indent=8, append_comma=True))
            if fixture.chunks:
                array_name = f"SIGN_TX_SEGMENTS_{prefix}_{fixture.sanitized_name}"
                lines.append(f"        .chunks = {array_name},")
                lines.append(f"        .chunk_count = ARRAY_LEN({array_name}),")
            else:
                lines.append("        .chunks = NULL,")
                lines.append("        .chunk_count = 0,")
            lines.append(f"        .expected_sw = {fixture.expected_sw},")
            lines.append(f"        .expect_init_failure = {'true' if fixture.expect_init_failure else 'false'},")
            lines.append("        .skip_reason = NULL,")
            lines.append("    },")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    exported = run_node_export()
    fixtures: Dict[str, List[FixtureInfo]] = {}
    for set_name, entries in exported.items():
        prefix = SET_PREFIX.get(set_name)
        if not prefix:
            continue
        fixtures.setdefault(set_name, [])
        for entry in entries:
            fixtures[set_name].append(build_fixture(entry, prefix))

    header = generate_header(fixtures)
    GENERATED_HEADER.write_text(header)
    print(f"Generated {GENERATED_HEADER}")


if __name__ == "__main__":
    main()
