# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
Minimal command builder used by the modernized test flows.

Provides chunks for the new handler_sign_tx protocol, the operational certificate
flow, and utility helpers shared by the standalone tests that still rely on this
module.
"""

import ipaddress

from dataclasses import dataclass
from enum import IntEnum
from typing import List, Optional

from ragger.bip import pack_derivation_path

from application_client.app_def import AddressType, StakingDataSourceType
from standalone.input_files.signOpCert import OpCertTestCase
from standalone.input_files.signTx import (
    AnchorParams,
    AuthorizeCommitteeParams,
    Certificate,
    CertificateType,
    CredentialParams,
    CredentialParamsType,
    DRepParams,
    DRepRegistrationParams,
    DRepUpdateParams,
    MultiHostRelayParams,
    PoolKey,
    PoolKeyType,
    PoolMetadataParams,
    PoolRegistrationParams,
    PoolRetirementParams,
    Relay,
    RelayType,
    ResignCommitteeParams,
    SingleHostHostnameRelayParams,
    SingleHostIpAddrRelayParams,
    StakeDelegationParams,
    StakeRegistrationConwayParams,
    StakeRegistrationParams,
    Transaction,
    TxAuxiliaryDataHash,
    TxAuxiliaryDataType,
    TxOutput,
    TxOutputBabbage,
    TxOutputDestinationType,
    TxRequiredSignerType,
    TransactionSigningMode,
    VoteDelegationParams,
    MAX_SIGN_TX_CHUNK_SIZE,
    Withdrawal,
)

CLA: int = 0xd7


class InsType(IntEnum):
    INS_GET_VERSION = 0x03
    INS_GET_APP_NAME = 0x04
    INS_GET_SERIAL = 0x01
    INS_GET_PUBLIC_KEY = 0x10
    INS_SIGN_TX = 0x21
    INS_SIGN_OPCERT = 0x22
    INS_DEBUG_SET_SETTINGS = 0xF0  # Debug-only command


class P1Type(IntEnum):
    P1_UNUSED = 0x00
    P1_RETURN = 0x01
    P1_DISPLAY = 0x02
    P1_TX_INIT = 0x00
    P1_TX_DATA_CHUNK = 0x01
    P1_TX_CHUNK_LAST = 0x02
    P1_TX_WITNESSES = 0x0F


class P2Type(IntEnum):
    P2_UNUSED = 0x00
    P2_MORE = 0x01
    P2_LAST = 0x02


def _credential_path_from_credential(credential: CredentialParams) -> Optional[str]:
    if credential.type.name == "KEY_PATH":
        return credential.keyValue
    return None


def _credential_path_from_certificate_params(params: object) -> Optional[str]:
    stake_credential = getattr(params, "stakeCredential", None)
    if stake_credential is not None:
        return _credential_path_from_credential(stake_credential)
    cold_credential = getattr(params, "coldCredential", None)
    if cold_credential is not None:
        return _credential_path_from_credential(cold_credential)
    drep_credential = getattr(params, "dRepCredential", None)
    if drep_credential is not None:
        return _credential_path_from_credential(drep_credential)
    pool_credential = getattr(params, "poolCredential", None)
    if pool_credential is not None:
        return _credential_path_from_credential(pool_credential)
    return None


def gather_witness_paths(tx: Transaction,
                         signing_mode: int,
                         additional_witness_paths: List[str]) -> List[str]:
    """Return unique witness paths present in a transaction."""

    witness_paths: List[str] = []

    if signing_mode == TransactionSigningMode.MULTISIG_TRANSACTION:
        for additional_path in additional_witness_paths:
            if additional_path not in witness_paths:
                witness_paths.append(additional_path)
        return witness_paths

    for tx_input in tx.inputs:
        if tx_input.path and tx_input.path not in witness_paths:
            witness_paths.append(tx_input.path)

    for certificate in tx.certificates:
        cert_path = _credential_path_from_certificate_params(certificate.params)
        if cert_path and cert_path not in witness_paths:
            witness_paths.append(cert_path)

    for withdrawal in tx.withdrawals:
        path = _credential_path_from_credential(withdrawal.stakeCredential)
        if path and path not in witness_paths:
            witness_paths.append(path)

    for additional_path in additional_witness_paths:
        if additional_path not in witness_paths:
            witness_paths.append(additional_path)

    return witness_paths


@dataclass(frozen=True)
class TxInitParams:
    options: int
    network_id: int
    protocol_magic: int
    signing_mode: int
    num_inputs: int
    num_outputs: int
    include_ttl: bool
    num_certificates: int
    num_withdrawals: int
    include_aux_data_hash: bool
    aux_data_hash_hex: Optional[str]
    include_validity_interval_start: bool
    num_mint_asset_groups: int
    include_script_data_hash: bool
    num_collateral_inputs: int
    num_required_signers: int
    include_network_id: bool
    include_collateral_output: bool
    include_total_collateral: bool
    num_reference_inputs: int
    num_voters: int
    include_treasury: bool
    include_donation: bool
    num_witnesses: int


class CommandBuilder:
    def _serialize(self,
                   ins: InsType,
                   p1: int = 0x00,
                   p2: int = 0x00,
                   cdata: bytes = bytes()) -> bytes:
        header = bytearray()
        header.append(CLA)
        header.append(ins)
        header.append(p1)
        header.append(p2)
        header.append(len(cdata))
        return header + cdata

    def get_version(self) -> bytes:
        return self._serialize(InsType.INS_GET_VERSION)

    def get_serial(self) -> bytes:
        return self._serialize(InsType.INS_GET_SERIAL)

    def get_pubkey_path(self, path: str) -> bytes:
        data = pack_derivation_path(path)
        return self._serialize(InsType.INS_GET_PUBLIC_KEY, P1Type.P1_UNUSED, P2Type.P2_UNUSED, data)

    def sign_opCert(self, testCase: OpCertTestCase) -> bytes:
        data = bytearray()
        data.extend(bytes.fromhex(testCase.opCert.kesPublicKeyHex))
        data.extend(testCase.opCert.kesPeriod.to_bytes(8, "big"))
        data.extend(testCase.opCert.issueCounter.to_bytes(8, "big"))
        data.extend(pack_derivation_path(testCase.opCert.path))
        return self._serialize(InsType.INS_SIGN_OPCERT, 0x00, 0x00, bytes(data))

    def sign_tx_init(self, params: TxInitParams) -> bytes:
        data = bytearray()
        data.extend(params.options.to_bytes(8, "big"))
        data.append(params.network_id)
        data.extend(params.protocol_magic.to_bytes(4, "big"))
        data.append(params.signing_mode)
        data.extend(params.num_inputs.to_bytes(2, "big"))
        data.extend(params.num_outputs.to_bytes(2, "big"))
        data.append(0x02 if params.include_ttl else 0x01)
        data.extend(params.num_certificates.to_bytes(2, "big"))
        data.extend(params.num_withdrawals.to_bytes(2, "big"))
        data.append(0x02 if params.include_aux_data_hash else 0x01)
        if params.include_aux_data_hash:
            if params.aux_data_hash_hex is None:
                raise ValueError("Auxiliary data hash is required when include_aux_data_hash is set")
            data.extend(bytes.fromhex(params.aux_data_hash_hex))
        data.append(0x02 if params.include_validity_interval_start else 0x01)
        data.extend(params.num_mint_asset_groups.to_bytes(2, "big"))
        data.append(0x02 if params.include_script_data_hash else 0x01)
        data.extend(params.num_collateral_inputs.to_bytes(2, "big"))
        data.extend(params.num_required_signers.to_bytes(2, "big"))
        data.append(0x02 if params.include_network_id else 0x01)
        data.append(0x02 if params.include_collateral_output else 0x01)
        data.append(0x02 if params.include_total_collateral else 0x01)
        data.extend(params.num_reference_inputs.to_bytes(2, "big"))
        data.extend(params.num_voters.to_bytes(2, "big"))
        data.append(0x02 if params.include_treasury else 0x01)
        data.append(0x02 if params.include_donation else 0x01)
        data.extend(params.num_witnesses.to_bytes(2, "big"))
        return self._serialize(InsType.INS_SIGN_TX, P1Type.P1_TX_INIT, P2Type.P2_UNUSED, bytes(data))

    def build_tx_init_params(self,
                             tx: Transaction,
                             signing_mode: int,
                             witness_paths: List[str],
                             options: int = 0) -> TxInitParams:
        include_aux_data_hash = (
            tx.auxiliaryData is not None and
            tx.auxiliaryData.type == TxAuxiliaryDataType.ARBITRARY_HASH
        )
        aux_data_hash_hex = None
        if include_aux_data_hash:
            aux_params = tx.auxiliaryData.params
            if isinstance(aux_params, TxAuxiliaryDataHash):
                aux_data_hash_hex = aux_params.hashHex
            else:
                include_aux_data_hash = False

        return TxInitParams(
            options=options,
            network_id=tx.network.networkId,
            protocol_magic=tx.network.protocol,
            signing_mode=signing_mode,
            num_inputs=len(tx.inputs),
            num_outputs=len(tx.outputs),
            include_ttl=tx.ttl is not None,
            num_certificates=len(tx.certificates),
            num_withdrawals=len(tx.withdrawals),
            include_aux_data_hash=include_aux_data_hash,
            aux_data_hash_hex=aux_data_hash_hex,
            include_validity_interval_start=tx.validityIntervalStart is not None,
            num_mint_asset_groups=len(tx.mint),
            include_script_data_hash=tx.scriptDataHash is not None,
            num_collateral_inputs=len(tx.collateralInputs) if getattr(tx, "collateralInputs", None) else 0,
            num_required_signers=len(tx.requiredSigners) if getattr(tx, "requiredSigners", None) else 0,
            include_network_id=bool(getattr(tx, "includeNetworkId", False)),
            include_collateral_output=getattr(tx, "collateralOutput", None) is not None,
            include_total_collateral=getattr(tx, "totalCollateral", None) is not None,
            num_reference_inputs=len(tx.referenceInputs) if getattr(tx, "referenceInputs", None) else 0,
            num_voters=len(tx.votingProcedures) if getattr(tx, "votingProcedures", None) else 0,
            include_treasury=getattr(tx, "treasury", None) is not None,
            include_donation=getattr(tx, "donation", None) is not None,
            num_witnesses=len(witness_paths),
        )

    def sign_tx_witness(self, path: str) -> bytes:
        data = pack_derivation_path(path)
        return self._serialize(InsType.INS_SIGN_TX, P1Type.P1_TX_WITNESSES, P2Type.P2_UNUSED, data)

    def debug_set_settings(self, expert_mode: bool, silent_export: bool) -> bytes:
        """Build debug settings APDU (only works with DEBUG builds).

        Args:
            expert_mode: True to enable expert mode, False to disable
            silent_export: True to enable silent pubkey export, False to disable

        Returns:
            Serialized APDU command
        """
        data = bytearray()
        data.append(0x01 if expert_mode else 0x00)
        data.append(0x01 if silent_export else 0x00)
        return self._serialize(InsType.INS_DEBUG_SET_SETTINGS, P1Type.P1_UNUSED, P2Type.P2_UNUSED, bytes(data))

    def serialize_transaction_chunks(self, tx: Transaction) -> list[bytes]:
        if MAX_SIGN_TX_CHUNK_SIZE <= 0:
            raise ValueError("MAX_SIGN_TX_CHUNK_SIZE must be positive")
        tx_data = self._serialize_transaction_unpacked_raw(tx)
        if not tx_data:
            raise ValueError("Serialized transaction must not be empty")
        chunks: List[bytes] = []
        offset = 0
        while offset < len(tx_data):
            chunk_size = min(MAX_SIGN_TX_CHUNK_SIZE, len(tx_data) - offset)
            chunk_data = tx_data[offset:offset + chunk_size]
            offset += chunk_size
            more = offset < len(tx_data)
            p1 = P1Type.P1_TX_DATA_CHUNK if more else P1Type.P1_TX_CHUNK_LAST
            chunk_apdu = self._serialize(InsType.INS_SIGN_TX, p1, P2Type.P2_UNUSED, chunk_data)
            chunks.append(chunk_apdu)
        return chunks

    def _serialize_transaction_unpacked_raw(self, tx: Transaction) -> bytes:
        data = bytearray()
        for tx_input in tx.inputs:
            data.extend(bytes.fromhex(tx_input.txHashHex))
            data.extend(tx_input.outputIndex.to_bytes(4, "big"))

        for tx_output in tx.outputs:
            output_data = self._serialize_output(tx_output, tx)
            data.extend(len(output_data).to_bytes(2, "big"))
            data.extend(output_data)

        data.extend(tx.fee.to_bytes(8, "big"))

        if tx.ttl is not None:
            data.extend(tx.ttl.to_bytes(8, "big"))

        for certificate in tx.certificates:
            data.extend(self._serialize_certificate(certificate))

        for withdrawal in tx.withdrawals:
            data.extend(withdrawal.amount.to_bytes(8, "big"))
            data.extend(self._serialize_credential_inline(withdrawal.stakeCredential))

        if tx.validityIntervalStart is not None:
            data.extend(tx.validityIntervalStart.to_bytes(8, "big"))

        for mint_asset_group in tx.mint:
            data.extend(bytes.fromhex(mint_asset_group.policyIdHex))
            data.extend(len(mint_asset_group.tokens).to_bytes(2, "big"))
            for token in mint_asset_group.tokens:
                asset_name_bytes = bytes.fromhex(token.assetNameHex)
                data.append(len(asset_name_bytes))
                data.extend(asset_name_bytes)
                data.extend(token.amount.to_bytes(8, "big", signed=True))

        script_data_hash = getattr(tx, "scriptDataHash", None)
        if script_data_hash is not None:
            data.extend(bytes.fromhex(script_data_hash))

        collateral_inputs = getattr(tx, "collateralInputs", None)
        if collateral_inputs:
            for collateral_input in collateral_inputs:
                data.extend(bytes.fromhex(collateral_input.txHashHex))
                data.extend(collateral_input.outputIndex.to_bytes(4, "big"))

        required_signers = getattr(tx, "requiredSigners", None)
        if required_signers:
            for required_signer in required_signers:
                if required_signer.type == TxRequiredSignerType.PATH:
                    data.append(0x00)
                    data.extend(pack_derivation_path(required_signer.pathOrHashHex))
                else:
                    data.append(0x01)
                    data.extend(bytes.fromhex(required_signer.pathOrHashHex))

        collateral_output = getattr(tx, "collateralOutput", None)
        if collateral_output is not None:
            collateral_data = self._serialize_output(collateral_output, tx)
            data.extend(len(collateral_data).to_bytes(2, "big"))
            data.extend(collateral_data)

        if getattr(tx, "totalCollateral", None) is not None:
            data.extend(tx.totalCollateral.to_bytes(8, "big"))

        reference_inputs = getattr(tx, "referenceInputs", None)
        if reference_inputs:
            for reference_input in reference_inputs:
                data.extend(bytes.fromhex(reference_input.txHashHex))
                data.extend(reference_input.outputIndex.to_bytes(4, "big"))

        voting_procedures = getattr(tx, "votingProcedures", None)
        if voting_procedures:
            for voter_votes in voting_procedures:
                # Serialize voter type
                data.append(voter_votes.voter.type)

                # Serialize voter data based on type
                if voter_votes.voter.type in [100, 102, 104]:  # KEY_PATH types
                    data.extend(pack_derivation_path(voter_votes.voter.keyValue))
                else:  # KEY_HASH or SCRIPT_HASH types
                    data.extend(bytes.fromhex(voter_votes.voter.keyValue))

                # Serialize number of votes for this voter
                data.extend(len(voter_votes.votes).to_bytes(2, "big"))

                # Serialize each vote
                for vote in voter_votes.votes:
                    # gov_action_id: tx_hash + index
                    data.extend(bytes.fromhex(vote.govActionId.txHashHex))
                    data.extend(vote.govActionId.govActionIndex.to_bytes(4, "big"))

                    # voting_procedure: vote option
                    data.append(vote.votingProcedure.vote)

                    # anchor inclusion flag
                    if vote.votingProcedure.anchor is not None:
                        data.append(0x02)  # ITEM_INCLUDED_YES

                        # anchor URL
                        anchor_url_bytes = vote.votingProcedure.anchor.url.encode('utf-8')
                        data.extend(len(anchor_url_bytes).to_bytes(2, "big"))
                        data.extend(anchor_url_bytes)

                        # anchor hash
                        data.extend(bytes.fromhex(vote.votingProcedure.anchor.hashHex))
                    else:
                        data.append(0x01)  # ITEM_INCLUDED_NO

        if getattr(tx, "treasury", None) is not None:
            data.extend(tx.treasury.to_bytes(8, "big"))

        if getattr(tx, "donation", None) is not None:
            data.extend(tx.donation.to_bytes(8, "big"))

        return bytes(data)

    def _serialize_output(self, tx_output: TxOutput, tx: Transaction) -> bytearray:
        output_data = bytearray()
        output_data.append(tx_output.destination.type)

        if tx_output.destination.type == TxOutputDestinationType.THIRD_PARTY:
            addr_bytes = bytes.fromhex(tx_output.destination.params.addressHex)
            output_data.extend(len(addr_bytes).to_bytes(2, "big"))
            output_data.extend(addr_bytes)
        else:
            addr_params = tx_output.destination.params
            output_data.append(addr_params.addrType)
            if addr_params.addrType == AddressType.BYRON:
                output_data.extend(addr_params.netDesc.protocol.to_bytes(4, "big"))
            else:
                output_data.append(tx.network.networkId)
            if addr_params.spendingValue.startswith("m/"):
                output_data.extend(pack_derivation_path(addr_params.spendingValue))
            else:
                output_data.extend(bytes.fromhex(addr_params.spendingValue))
            if addr_params.addrType in (AddressType.BYRON, AddressType.ENTERPRISE_KEY,
                                        AddressType.ENTERPRISE_SCRIPT):
                staking_choice = StakingDataSourceType.NONE
            elif addr_params.addrType in (AddressType.BASE_PAYMENT_KEY_STAKE_SCRIPT,
                                          AddressType.BASE_PAYMENT_SCRIPT_STAKE_SCRIPT,
                                          AddressType.REWARD_SCRIPT):
                staking_choice = StakingDataSourceType.SCRIPT_HASH
            elif addr_params.addrType in (AddressType.POINTER_KEY, AddressType.POINTER_SCRIPT):
                staking_choice = StakingDataSourceType.BLOCKCHAIN_POINTER
            elif addr_params.stakingValue.startswith("m/"):
                staking_choice = StakingDataSourceType.KEY_PATH
            else:
                staking_choice = StakingDataSourceType.KEY_HASH
            output_data.append(staking_choice)
            if staking_choice == StakingDataSourceType.KEY_PATH:
                output_data.extend(pack_derivation_path(addr_params.stakingValue))
            elif staking_choice in (StakingDataSourceType.KEY_HASH, StakingDataSourceType.SCRIPT_HASH):
                output_data.extend(bytes.fromhex(addr_params.stakingValue))
            elif staking_choice == StakingDataSourceType.BLOCKCHAIN_POINTER:
                output_data.extend(bytes.fromhex(addr_params.stakingValue))

        output_data.extend(tx_output.amount.to_bytes(8, "big"))
        output_data.append(tx_output.format if hasattr(tx_output, "format") else 0)
        num_asset_groups = len(tx_output.tokenBundle) if hasattr(tx_output, "tokenBundle") else 0
        output_data.extend(num_asset_groups.to_bytes(2, "big"))

        if num_asset_groups > 0:
            for asset_group in tx_output.tokenBundle:
                output_data.extend(bytes.fromhex(asset_group.policyIdHex))
                output_data.extend(len(asset_group.tokens).to_bytes(2, "big"))
                for token in asset_group.tokens:
                    asset_name_bytes = bytes.fromhex(token.assetNameHex)
                    output_data.append(len(asset_name_bytes))
                    output_data.extend(asset_name_bytes)
                    output_data.extend(token.amount.to_bytes(8, "big"))

        if hasattr(tx_output, "datum") and tx_output.datum is not None:
            datum_type = tx_output.datum.type
            if datum_type == 0:
                output_data.append(0x01)
                output_data.extend(bytes.fromhex(tx_output.datum.datumHex))
            elif datum_type == 1:
                output_data.append(0x02)
                datum_bytes = bytes.fromhex(tx_output.datum.datumHex)
                output_data.extend(len(datum_bytes).to_bytes(2, "big"))
                output_data.extend(datum_bytes)
        else:
            output_data.append(0x00)

        if isinstance(tx_output, TxOutputBabbage) and tx_output.referenceScriptHex is not None:
            output_data.append(0x02)
            script_bytes = bytes.fromhex(tx_output.referenceScriptHex)
            output_data.extend(len(script_bytes).to_bytes(2, "big"))
            output_data.extend(script_bytes)
        else:
            output_data.append(0x00)

        return output_data

    def _serialize_credential_inline(self, credential: CredentialParams) -> bytes:
        data = bytearray()
        if credential.keyValue is None:
            raise ValueError("Credential keyValue must be set")
        if credential.type == CredentialParamsType.KEY_PATH:
            data.append(0x02)
            data.extend(pack_derivation_path(credential.keyValue))
        elif credential.type == CredentialParamsType.KEY_HASH:
            data.append(0x00)
            data.extend(bytes.fromhex(credential.keyValue))
        elif credential.type == CredentialParamsType.SCRIPT_HASH:
            data.append(0x01)
            data.extend(bytes.fromhex(credential.keyValue))
        else:
            raise ValueError(f"Unsupported credential type: {credential.type}")
        return bytes(data)

    def _serialize_drep(self, drep: DRepParams) -> bytes:
        result = bytearray()
        result.append(int(drep.type))
        if drep.keyValue is not None:
            if drep.keyValue.startswith("m/"):
                result.extend(pack_derivation_path(drep.keyValue))
            else:
                result.extend(bytes.fromhex(drep.keyValue))
        return bytes(result)

    def _serialize_anchor(self, anchor: Optional[AnchorParams]) -> bytes:
        result = bytearray()
        if anchor is None:
            result.append(0x00)
            return bytes(result)

        result.append(0x01)
        url_bytes = anchor.url.encode("utf-8")
        if len(url_bytes) > 0xFF:
            raise ValueError("Anchor URL exceeds maximum length")
        result.append(len(url_bytes))
        result.extend(url_bytes)
        hash_bytes = bytes.fromhex(anchor.hashHex)
        if len(hash_bytes) != 32:
            raise ValueError("Anchor hash must be 32 bytes")
        result.extend(hash_bytes)
        return bytes(result)

    def _pool_key_to_credential(self, pool_key: PoolKey) -> CredentialParams:
        if pool_key.type == PoolKeyType.DEVICE_OWNED:
            return CredentialParams(type=CredentialParamsType.KEY_PATH, keyValue=pool_key.key)
        if pool_key.type == PoolKeyType.THIRD_PARTY:
            return CredentialParams(type=CredentialParamsType.KEY_HASH, keyValue=pool_key.key.lower())
        raise ValueError(f"Unsupported pool key type: {pool_key.type}")

    def _serialize_pool_key_reference(self, pool_key: PoolKey) -> bytes:
        data = bytearray()
        if pool_key.type == PoolKeyType.DEVICE_OWNED:
            data.append(0x02)
            data.extend(pack_derivation_path(pool_key.key))
        elif pool_key.type == PoolKeyType.THIRD_PARTY:
            data.append(0x00)
            data.extend(bytes.fromhex(pool_key.key.lower()))
        else:
            raise ValueError(f"Unsupported pool key type: {pool_key.type}")
        return bytes(data)

    def _serialize_relay(self, relay: Relay) -> bytes:
        data = bytearray()
        data.append(int(relay.type))
        if relay.type == RelayType.SINGLE_HOST_IP_ADDR:
            params = relay.params
            assert isinstance(params, SingleHostIpAddrRelayParams)
            if params.portNumber is None:
                data.append(0x00)
            else:
                data.append(0x02)
                data.extend(params.portNumber.to_bytes(2, "big"))
            if not params.ipv4:
                data.append(0x00)
            else:
                data.append(0x02)
                data.extend(ipaddress.IPv4Address(params.ipv4).packed)
            if not params.ipv6:
                data.append(0x00)
            else:
                data.append(0x02)
                data.extend(ipaddress.IPv6Address(params.ipv6).packed)
        elif relay.type == RelayType.SINGLE_HOST_HOSTNAME:
            params = relay.params
            assert isinstance(params, SingleHostHostnameRelayParams)
            if params.portNumber is None:
                data.append(0x00)
            else:
                data.append(0x02)
                data.extend(params.portNumber.to_bytes(2, "big"))
            dns_bytes = (params.dnsName or "").encode("utf-8")
            if len(dns_bytes) > 0xFF:
                raise ValueError("Relay DNS name exceeds maximum length")
            data.append(len(dns_bytes))
            data.extend(dns_bytes)
        elif relay.type == RelayType.MULTI_HOST:
            params = relay.params
            assert isinstance(params, MultiHostRelayParams)
            dns_bytes = (params.dnsName or "").encode("utf-8")
            if len(dns_bytes) > 0xFF:
                raise ValueError("Relay DNS name exceeds maximum length")
            data.append(len(dns_bytes))
            data.extend(dns_bytes)
        else:
            raise ValueError(f"Unsupported relay type: {relay.type}")
        return bytes(data)

    def _serialize_pool_metadata(self, metadata: Optional[PoolMetadataParams]) -> bytes:
        data = bytearray()
        if metadata is None:
            data.append(0x00)
            return bytes(data)
        url_bytes = metadata.metadataUrl.encode("utf-8")
        if len(url_bytes) > 0xFF:
            raise ValueError("Pool metadata URL exceeds maximum length")
        hash_bytes = bytes.fromhex(metadata.metadataHashHex.lower())
        if len(hash_bytes) != 32:
            raise ValueError("Pool metadata hash must be 32 bytes")
        data.append(0x01)
        data.append(len(url_bytes))
        data.extend(url_bytes)
        data.extend(hash_bytes)
        return bytes(data)

    def _serialize_pool_registration(self, params: PoolRegistrationParams) -> bytes:
        data = bytearray()
        data.extend(self._serialize_pool_key_reference(params.poolKey))
        vrf_bytes = bytes.fromhex(params.vrfKeyHashHex.lower())
        if len(vrf_bytes) != 32:
            raise ValueError("VRF key hash must be 32 bytes")
        data.extend(vrf_bytes)
        data.extend(params.pledge.to_bytes(8, "big"))
        data.extend(params.cost.to_bytes(8, "big"))
        data.extend(params.margin.numerator.to_bytes(8, "big"))
        data.extend(params.margin.denominator.to_bytes(8, "big"))
        data.extend(self._serialize_pool_key_reference(params.rewardAccount))
        data.append(len(params.poolOwners))
        for owner in params.poolOwners:
            credential = self._pool_key_to_credential(owner)
            data.extend(self._serialize_credential_inline(credential))
        data.append(len(params.relays))
        for relay in params.relays:
            data.extend(self._serialize_relay(relay))
        data.extend(self._serialize_pool_metadata(params.metadata))
        return bytes(data)

    def _serialize_certificate(self, certificate: Certificate) -> bytes:
        result = bytearray()
        result.append(int(certificate.type))

        cert_type = certificate.type
        params = certificate.params

        if cert_type in (CertificateType.STAKE_REGISTRATION, CertificateType.STAKE_DEREGISTRATION):
            assert isinstance(params, StakeRegistrationParams)
            result.extend(self._serialize_credential_inline(params.stakeCredential))
        elif cert_type in (CertificateType.STAKE_REGISTRATION_CONWAY, CertificateType.STAKE_DEREGISTRATION_CONWAY):
            assert isinstance(params, StakeRegistrationConwayParams)
            result.extend(self._serialize_credential_inline(params.stakeCredential))
            result.extend(params.deposit.to_bytes(8, "big"))
        elif cert_type == CertificateType.STAKE_DELEGATION:
            assert isinstance(params, StakeDelegationParams)
            result.extend(self._serialize_credential_inline(params.stakeCredential))
            result.extend(bytes.fromhex(params.poolKeyHash))
        elif cert_type == CertificateType.VOTE_DELEGATION:
            assert isinstance(params, VoteDelegationParams)
            result.extend(self._serialize_credential_inline(params.stakeCredential))
            result.extend(self._serialize_drep(params.dRep))
        elif cert_type == CertificateType.AUTHORIZE_COMMITTEE_HOT:
            assert isinstance(params, AuthorizeCommitteeParams)
            result.extend(self._serialize_credential_inline(params.coldCredential))
            result.extend(self._serialize_credential_inline(params.hotCredential))
        elif cert_type == CertificateType.RESIGN_COMMITTEE_COLD:
            assert isinstance(params, ResignCommitteeParams)
            result.extend(self._serialize_credential_inline(params.coldCredential))
            result.extend(self._serialize_anchor(params.anchor))
        elif cert_type == CertificateType.DREP_REGISTRATION:
            assert isinstance(params, DRepRegistrationParams)
            result.extend(self._serialize_credential_inline(params.dRepCredential))
            result.extend(params.deposit.to_bytes(8, "big"))
            result.extend(self._serialize_anchor(params.anchor))
        elif cert_type == CertificateType.DREP_DEREGISTRATION:
            assert isinstance(params, DRepRegistrationParams)
            result.extend(self._serialize_credential_inline(params.dRepCredential))
            result.extend(params.deposit.to_bytes(8, "big"))
        elif cert_type == CertificateType.DREP_UPDATE:
            assert isinstance(params, DRepUpdateParams)
            result.extend(self._serialize_credential_inline(params.dRepCredential))
            result.extend(self._serialize_anchor(params.anchor))
        elif cert_type == CertificateType.STAKE_POOL_REGISTRATION:
            assert isinstance(params, PoolRegistrationParams)
            result.extend(self._serialize_pool_registration(params))
        elif cert_type == CertificateType.STAKE_POOL_RETIREMENT:
            assert isinstance(params, PoolRetirementParams)
            result.extend(self._serialize_credential_inline(params.poolCredential))
            result.extend(params.retirementEpoch.to_bytes(8, "big"))
        else:
            raise ValueError(f"Unsupported certificate type: {cert_type}")

        return bytes(result)
