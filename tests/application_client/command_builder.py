# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests Client application.
It contains the command building part.
"""

from enum import IntEnum
from typing import List, Optional

from ragger.bip import pack_derivation_path

from standalone.input_files.derive_address import DeriveAddressTestCase
from standalone.input_files.cvote import MAX_CIP36_PAYLOAD_SIZE, CVoteTestCase
from standalone.input_files.signOpCert import OpCertTestCase
from standalone.input_files.signMsg import SignMsgTestCase, MessageAddressFieldType
from standalone.input_files.signTx import SignTxTestCase, TxInput, TxOutput, Certificate, Withdrawal, Transaction
from standalone.input_files.signTx import TxAuxiliaryData, TxAuxiliaryDataHash, DRepParams
from standalone.input_files.signTx import TxOutputDestinationType, TxOutputDestination
from standalone.input_files.signTx import TxOutputBabbage, ThirdPartyAddressParams
from standalone.input_files.signTx import CertificateType, CredentialParams, CredentialParamsType, RequiredSigner
from standalone.input_files.signTx import VoterVotes, AnchorParams, TxAuxiliaryDataCIP36, CIP36VoteDelegation
from standalone.input_files.signTx import CIP36VoteDelegationType, AssetGroup, Token, Datum, DatumType
from standalone.input_files.signTx import PoolRetirementParams, DRepUpdateParams, DRepRegistrationParams
from standalone.input_files.signTx import ResignCommitteeParams, AuthorizeCommitteeParams, VoteDelegationParams
from standalone.input_files.signTx import StakeDelegationParams, StakeRegistrationConwayParams, StakeRegistrationParams
from standalone.input_files.signTx import PoolRegistrationParams, PoolKey, Relay, PoolMetadataParams, RelayType
from standalone.input_files.signTx import SingleHostIpAddrRelayParams, SingleHostHostnameRelayParams, MultiHostRelayParams
from standalone.input_files.signTx import MAX_SIGN_TX_CHUNK_SIZE
from standalone.input_files.derive_native_script import NativeScript, NativeScriptType, NativeScriptHashDisplayFormat
from standalone.input_files.derive_native_script import NativeScriptParamsPubkey, NativeScriptParamsInvalid
from standalone.input_files.derive_native_script import NativeScriptParamsScripts, NativeScriptParamsNofK

from application_client.app_def import AddressType, StakingDataSourceType

CLA: int = 0xd7

class InsType(IntEnum):
    GET_VERSION = 0x03
    GET_APP_NAME = 0x04
    GET_SERIAL = 0x01
    GET_PUBLIC_KEY = 0x10
    DERIVE_PUBLIC_ADDR = 0x11
    DERIVE_SCRIPT_HASH = 0x12
    SIGN_TX = 0x21
    SIGN_OP_CERT = 0x22
    SIGN_CIP36_VOTE = 0x23
    SIGN_MSG = 0x24

class P1Type(IntEnum):
    P2_UNUSED = 0x00
    P1_START = 0x00

    # SignTx new protocol (P1 controls flow, P2 is always UNUSED)
    P1_TX_INIT = 0x00
    P1_TX_DATA_CHUNK = 0x01
    P1_TX_CHUNK_LAST = 0x02

    # Derive Address
    P1_RETURN = 0x01
    P1_DISPLAY = 0x02
    # Get Pub Key
    P1_KEY_NEXT = 0x00
    P1_KEY_INIT = 0x01
    # Sign CIP36 Vote
    P1_INIT = 0x01
    P1_CHUNK = 0x02
    P1_CONFIRM = 0x03
    P1_WITNESS = 0x04
    # SignTx
    P1_INPUTS = 0x02
    P1_OUTPUTS = 0x03
    P1_FEE = 0x04
    P1_TTL = 0x05
    P1_CERTIFICATES = 0x06
    P1_WITHDRAWALS = 0x07
    P1_AUX_DATA = 0x08
    P1_VALIDITY_INTERVAL_START = 0x09
    P1_TX_CONFIRM = 0x0a
    P1_MINT = 0x0b
    P1_SCRIPT_DATA_HASH = 0x0c
    P1_COLLATERAL_INPUTS = 0x0d
    P1_REQUIRED_SIGNERS = 0x0e
    P1_TX_WITNESSES = 0x0f
    P1_TOTAL_COLLATERAL = 0x10
    P1_REFERENCE_INPUTS = 0x11
    P1_COLLATERAL_OUTPUT = 0x12
    P1_VOTING_PROCEDURES = 0x13
    P1_TREASURY = 0x15
    P1_DONATION = 0x16
    # Derive Native Script Hash
    P1_COMPLEX_SCRIPT_START = 0x01
    P1_ADD_SIMPLE_SCRIPT = 0x02
    P1_WHOLE_NATIVE_SCRIPT_FINISH = 0x03


class P2Type(IntEnum):
    P2_NONE = 0x00
    # SignTx Outputs
    P2_BASIC_DATA = 0x30
    P2_DATUM = 0x34
    P2_DATUM_CHUNK = 0x35
    P2_SCRIPT = 0x36
    P2_SCRIPT_CHUNK = 0x37
    P2_CONFIRM = 0x33
    # SignTx Aux Data
    P2_INIT = 0x36
    P2_VOTE_KEY = 0x30
    P2_DELEGATION = 0x37
    P2_STAKING_KEY = 0x31
    P2_PAYMENT_ADDRESS = 0x32
    P2_NONCE = 0x33
    P2_VOTING_PURPOSE = 0x35
    P2_AUX_CONFIRM = 0x34
    # SignTx Asset Group
    ASSET_GROUP = 0x31
    TOKEN = 0x32
    # SignTx Certificates
    P2_CERT_INIT = 0x30
    P2_POOL_KEY = 0x31
    P2_VRF_KEY = 0x32
    P2_FINANCIALS = 0x33
    P2_REWARD_ACCOUNT = 0x34
    P2_OWNERS = 0x35
    P2_RELAYS = 0x36
    P2_METADATA = 0x37
    P2_CERT_CONFIRM = 0x38


def gather_witness_paths(tx: Transaction, additional_witness_paths: List[str]) -> List[str]:
    """Gather unique witness paths from transaction elements.

    Collects paths from:
    - Input paths
    - Certificate paths (by credential type)
    - Withdrawal paths
    - Additional witness paths from test case

    Duplicates are removed while preserving order.

    Args:
        tx: The transaction object
        additional_witness_paths: List of additional witness paths from test case

    Returns:
        List of unique witness paths in order of first occurrence
    """
    witness_paths = []

    # Add input paths
    for tx_input in tx.inputs:
        if tx_input.path and tx_input.path not in witness_paths:
            witness_paths.append(tx_input.path)

    # Add certificate paths
    for certificate in tx.certificates:
        cert_path = None
        # Different certificate types have different path locations
        if hasattr(certificate.params, 'stakeCredential'):
            if certificate.params.stakeCredential.type.name == 'KEY_PATH':
                cert_path = certificate.params.stakeCredential.keyValue
        elif hasattr(certificate.params, 'coldCredential'):
            if certificate.params.coldCredential.type.name == 'KEY_PATH':
                cert_path = certificate.params.coldCredential.keyValue
        elif hasattr(certificate.params, 'dRepCredential'):
            if certificate.params.dRepCredential.type.name == 'KEY_PATH':
                cert_path = certificate.params.dRepCredential.keyValue
        elif hasattr(certificate.params, 'poolCredential'):
            if certificate.params.poolCredential.type.name == 'KEY_PATH':
                cert_path = certificate.params.poolCredential.keyValue

        if cert_path and cert_path not in witness_paths:
            witness_paths.append(cert_path)

    # Add withdrawal paths
    for withdrawal in tx.withdrawals:
        if withdrawal.stakeCredential.type.name == 'KEY_PATH':
            path = withdrawal.stakeCredential.keyValue
            if path not in witness_paths:
                witness_paths.append(path)

    # Add additional witness paths from test case
    for additional_path in additional_witness_paths:
        if additional_path not in witness_paths:
            witness_paths.append(additional_path)

    return witness_paths


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
        """APDU Builder for App version"""

        return self._serialize(InsType.GET_VERSION)


    def get_serial(self) -> bytes:
        """APDU Builder for App serial"""

        return self._serialize(InsType.GET_SERIAL)


    def derive_address(self, p1: P1Type, testCase: DeriveAddressTestCase) -> bytes:
        """APDU Builder for Derive Address

        Args:
            p1 (P1Type): APDU Parameter 1
            testCase (DeriveAddressTestCase): Test parameters

        Returns:
            Serial data APDU
        """

        data = self._serializeAddressParams(testCase)
        return self._serialize(InsType.DERIVE_PUBLIC_ADDR, p1, 0x00, data)


    def get_pubkey_init(self, num_keys: int) -> bytes:
        """APDU Builder for Public Key init

        Args:
            num_keys (int): Number of keys to export

        Returns:
            Response APDU
        """
        data = num_keys.to_bytes(2, "big")
        return self._serialize(InsType.GET_PUBLIC_KEY, P1Type.P1_KEY_INIT, P2Type.P2_NONE, data)

    def get_pubkey_path(self, path: str) -> bytes:
        """APDU Builder for Public Key path

        Args:
            path (str): Derivation path
        """
        data = pack_derivation_path(path)
        return self._serialize(InsType.GET_PUBLIC_KEY, P1Type.P1_KEY_NEXT, P2Type.P2_NONE, data)

    def sign_cip36_init(self, testCase: CVoteTestCase) -> bytes:
        """APDU Builder for CIP36 Vote - INIT step

        Args:
            testCase (CVoteTestCase): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Full length of voteCastDataHex (4B)
        #    voteCastDataHex (up to 240 B)
        data = bytes()
        # 2 hex chars per byte
        data_size = int(len(testCase.cVote.voteCastDataHex) / 2)
        chunk_size = min(MAX_CIP36_PAYLOAD_SIZE * 2, len(testCase.cVote.voteCastDataHex))
        data += data_size.to_bytes(4, "big")
        data += bytes.fromhex(testCase.cVote.voteCastDataHex[:chunk_size])
        # Remove the data sent in this step
        testCase.cVote.voteCastDataHex = testCase.cVote.voteCastDataHex[chunk_size:]
        return self._serialize(InsType.SIGN_CIP36_VOTE, P1Type.P1_INIT, 0x00, data)


    def sign_cip36_chunk(self, testCase: CVoteTestCase) -> List[bytes]:
        """APDU Builder for CIP36 Vote - CHUNK step

        Args:
            testCase (CVoteTestCase): Test parameters

        Returns:
            Response APDU
        """

        # Serialization format:
        #    voteCastDataHex (following data, up to MAX_CIP36_PAYLOAD_SIZE B each)
        chunks = []
        payload = testCase.cVote.voteCastDataHex
        max_payload_size = MAX_CIP36_PAYLOAD_SIZE * 2 # 2 hex chars per byte
        while len(payload) > 0:
            chunks.append(self._serialize(InsType.SIGN_CIP36_VOTE,
                                          P1Type.P1_CHUNK,
                                          0x00,
                                          bytes.fromhex(payload[:max_payload_size])))
            payload = payload[max_payload_size:]

        return chunks


    def sign_cip36_confirm(self) -> bytes:
        """APDU Builder for CIP36 Vote - CONFIRM step

        Returns:
            Serial data APDU
        """

        return self._serialize(InsType.SIGN_CIP36_VOTE, P1Type.P1_CONFIRM, 0x00)


    def sign_cip36_witness(self, testCase: CVoteTestCase) -> bytes:
        """APDU Builder for CIP36 Vote - WITNESS step

        Args:
            testCase (CVoteTestCase): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #     witness path (1B for length + [0-10] x 4B)
        return self._serialize(InsType.SIGN_CIP36_VOTE,
                               P1Type.P1_WITNESS,
                               0x00,
                               pack_derivation_path(testCase.cVote.witnessPath))


    def sign_opCert(self, testCase: OpCertTestCase) -> bytes:
        """APDU Builder for Sign Operational Certificate

        Args:
            testCase (OpCertTestCase): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        # kesPublicKeyHex hex string (32B)
        # kesPeriod (8B)
        # issueCounter (8B)
        # derivation path (1B for length + [0-10] x 4B)
        data = bytes()
        data += bytes.fromhex(testCase.opCert.kesPublicKeyHex)
        data += testCase.opCert.kesPeriod.to_bytes(8, "big")
        data += testCase.opCert.issueCounter.to_bytes(8, "big")
        data += pack_derivation_path(testCase.opCert.path)
        return self._serialize(InsType.SIGN_OP_CERT, 0x00, 0x00, data)


    def sign_msg_init(self, testCase: SignMsgTestCase) -> bytes:
        """APDU Builder for Sign Message - INIT step

        Args:
            testCase (SignMsgTestCase): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Full length of messageHex (4B)
        #    signingPath (1B for length + [0-10] x 4B)
        #    hashPayload (1B)
        #    isAscii display (1B)
        #    addressFieldType (1B)
        #    addressBuffer, if any
        data = bytes()
        # 2 hex chars per byte
        data_size = int(len(testCase.msgData.messageHex) / 2)
        data += data_size.to_bytes(4, "big")
        data += pack_derivation_path(testCase.msgData.signingPath)

        data += testCase.msgData.hashPayload.to_bytes(1, "big")
        data += testCase.msgData.isAscii.to_bytes(1, "big")
        data += testCase.msgData.addressFieldType.to_bytes(1, "big")
        if testCase.msgData.addressFieldType == MessageAddressFieldType.ADDRESS:
            assert testCase.msgData.addressDesc is not None
            data += self._serializeAddressParams(testCase.msgData.addressDesc)
        return self._serialize(InsType.SIGN_MSG, P1Type.P1_INIT, 0x00, data)


    def sign_msg_chunk(self, testCase: SignMsgTestCase) -> List[bytes]:
        """APDU Builder for Sign Message - CHUNK step

        Args:
            testCase (SignMsgTestCase): Test parameters

        Returns:
            Response APDU
        """

        MAX_CIP8_MSG_FIRST_CHUNK_ASCII_SIZE = 198 * 2
        MAX_CIP8_MSG_FIRST_CHUNK_HEX_SIZE = 99 * 2
        MAX_CIP8_MSG_HIDDEN_CHUNK_SIZE = 250 * 2
        # Serialization format:
        #    messageHex (up to MAX_CIP8_MSG_HIDDEN_CHUNK_SIZE B each, started by the length)
        chunks = []
        if testCase.msgData.isAscii:
            firstChunkSize = MAX_CIP8_MSG_FIRST_CHUNK_ASCII_SIZE
        else:
            firstChunkSize = MAX_CIP8_MSG_FIRST_CHUNK_HEX_SIZE
        chunk_size = min(firstChunkSize, len(testCase.msgData.messageHex))
        payload = testCase.msgData.messageHex
        while True:
            data = bytes()
            data += int(chunk_size / 2).to_bytes(4, "big")
            data += bytes.fromhex(payload[:chunk_size])
            chunks.append(self._serialize(InsType.SIGN_MSG, P1Type.P1_CHUNK, 0x00, data))
            payload = payload[chunk_size:]
            chunk_size = min(MAX_CIP8_MSG_HIDDEN_CHUNK_SIZE, len(payload))
            if len(payload) == 0:
                break

        return chunks


    def sign_msg_confirm(self) -> bytes:
        """APDU Builder for Sign Message - CONFIRM step

        Returns:
            Serial data APDU
        """

        return self._serialize(InsType.SIGN_MSG, P1Type.P1_CONFIRM, 0x00)


    def sign_tx_init(self, testCase: SignTxTestCase, nbWitnessPaths: int) -> bytes:
        """APDU Builder for Sign TX - INIT step

        Args:
            testCase (SignTxTestCase): Test parameters
            nbWitnessPaths (int): The number of unique witness paths

        Returns:
            Serial data APDU

        Note: Format follows canonical CBOR transaction body ordering from Cardano spec
        """

        # Serialization format (following CBOR transaction_body ordering):
        #    Options (8B)
        #    NetworkId (1B)
        #    ProtocolMagic (4B)
        #    Signing mode (1B)
        #
        #    TX inputs length (4B)                  // field 0
        #    TX outputs length (4B)                 // field 1
        #
        #    Include TTL flag (1B)                  // field 3 (optional)
        #    TX certificates length (4B)            // field 4 (optional, placeholder)
        #    TX withdrawals length (4B)             // field 5 (optional)
        #
        #    Include auxiliary data hash flag (1B)   // field 7 (optional)
        #    Include validityIntervalStart flag (1B) // field 8 (optional)
        #
        #    TX mint asset groups count (4B)        // field 9 (optional)
        #
        #    Include scriptDataHash flag (1B)       // field 11 (optional)
        #    TX collateralInputs length (4B)        // field 13 (optional)
        #    TX requiredSigners length (4B)         // field 14 (optional)
        #    Include networkId flag (1B)            // field 15 (optional)
        #    Include collateralOutput flag (1B)     // field 16 (optional)
        #    Include totalCollateral flag (1B)      // field 17 (optional)
        #    TX referenceInputs length (4B)         // field 18 (optional)
        #    TX votingProcedures length (4B)        // field 19 (optional)
        #    Include treasury flag (1B)             // field 21 (optional)
        #    Include donation flag (1B)             // field 22 (optional)
        #
        #    Number of witness paths (4B)

        data = bytes()

        # Fixed header
        data += testCase.options.to_bytes(8, "big")
        data += testCase.tx.network.networkId.to_bytes(1, "big")
        data += testCase.tx.network.protocol.to_bytes(4, "big")
        data += testCase.signingMode.to_bytes(1, "big")

        # Fields 0-1: inputs and outputs (always present)
        data += len(testCase.tx.inputs).to_bytes(2, "big")
        data += len(testCase.tx.outputs).to_bytes(2, "big")

        # Field 3 (TTL) - optional
        data += self._serializeOptionFlags(testCase.tx.ttl is not None)
        # Field 4 (certificates) - optional, placeholder for future
        data += len(testCase.tx.certificates).to_bytes(2, "big")
        # Field 5 (withdrawals) - optional
        data += len(testCase.tx.withdrawals).to_bytes(2, "big")

        # Field 7 (auxiliary data hash) - optional
        data += self._serializeOptionFlags(testCase.tx.auxiliaryData is not None)
        # Field 8 (validity interval start) - optional
        data += self._serializeOptionFlags(testCase.tx.validityIntervalStart is not None)

        # Field 9 (mint) - optional
        data += len(testCase.tx.mint).to_bytes(2, "big")

        # Field 11 (script data hash) - optional
        data += self._serializeOptionFlags(testCase.tx.scriptDataHash is not None)
        # Field 13 (collateral inputs) - optional
        data += len(testCase.tx.collateralInputs).to_bytes(2, "big")
        # Field 14 (required signers) - optional
        data += len(testCase.tx.requiredSigners).to_bytes(2, "big")
        # Field 15 (network ID) - optional
        data += self._serializeOptionFlags(testCase.tx.includeNetworkId is not None)
        # Field 16 (collateral output) - optional
        data += self._serializeOptionFlags(testCase.tx.collateralOutput is not None)
        # Field 17 (total collateral) - optional
        data += self._serializeOptionFlags(testCase.tx.totalCollateral is not None)
        # Field 18 (reference inputs) - optional
        data += len(testCase.tx.referenceInputs).to_bytes(2, "big")
        # Field 19 (voting procedures) - optional
        data += len(testCase.tx.votingProcedures).to_bytes(2, "big")
        # Field 21 (treasury) - optional
        data += self._serializeOptionFlags(testCase.tx.treasury is not None)
        # Field 22 (donation) - optional
        data += self._serializeOptionFlags(testCase.tx.donation is not None)

        # Number of witness paths
        data += nbWitnessPaths.to_bytes(4, "big")

        return self._serialize(InsType.SIGN_TX, P1Type.P1_INIT, 0x00, data)


    def sign_tx_aux_data_serialize(self, auxData: TxAuxiliaryData) -> bytes:
        """APDU Builder for Sign TX - AUX DATA step - SERIALIZE mode

        Args:
            auxData (TxAuxiliaryData): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Type (1B)
        #    HashHex bytes
        data = bytes()
        data += auxData.type.to_bytes(1, "big")
        if isinstance(auxData.params, TxAuxiliaryDataHash):
            data += bytes.fromhex(auxData.params.hashHex)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_AUX_DATA, 0x00, data)


    def sign_tx_aux_data_init(self, auxData: TxAuxiliaryDataCIP36) -> bytes:
        """APDU Builder for Sign TX - AUX DATA step - INIT mode

        Args:
            auxData (TxAuxiliaryDataCIP36): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Type (1B)
        #    Nb of delegations (4B)
        data = bytes()
        data += auxData.format.to_bytes(1, "big")
        numDelegation = len(auxData.delegations)
        data += numDelegation.to_bytes(4, "big")
        return self._serialize(InsType.SIGN_TX, P1Type.P1_AUX_DATA, P2Type.P2_INIT, data)


    def sign_tx_aux_data_vote_key(self, auxData: TxAuxiliaryDataCIP36) -> bytes:
        """APDU Builder for Sign TX - AUX DATA step - VOTE KEY mode

        Args:
            auxData (TxAuxiliaryDataCIP36): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Type (1B)
        #    Vote Key Hex
        data = bytes()
        assert auxData.voteKey is not None
        if auxData.voteKey.startswith("m/"):
            data += CIP36VoteDelegationType.PATH.to_bytes(1, "big")
            data += pack_derivation_path(auxData.voteKey)
        else:
            data += CIP36VoteDelegationType.KEY.to_bytes(1, "big")
            data += bytes.fromhex(auxData.voteKey)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_AUX_DATA, P2Type.P2_VOTE_KEY, data)


    def sign_tx_aux_data_delegation(self, delegation: CIP36VoteDelegation) -> bytes:
        """APDU Builder for Sign TX - AUX DATA step - DELEGATION mode

        Args:
            delegation (CIP36VoteDelegation): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Type (1B)
        #    Vote Key Hex
        #    Weight (4B)
        data = bytes()
        data += delegation.type.to_bytes(1, "big")
        if delegation.votingKeyPath.startswith("m/"):
            data += pack_derivation_path(delegation.votingKeyPath)
        else:
            data += bytes.fromhex(delegation.votingKeyPath)
        data += delegation.weight.to_bytes(4, "big")
        return self._serialize(InsType.SIGN_TX, P1Type.P1_AUX_DATA, P2Type.P2_DELEGATION, data)


    def sign_tx_aux_data_staking(self, auxData: TxAuxiliaryDataCIP36) -> bytes:
        """APDU Builder for Sign TX - AUX DATA step - STAKING mode

        Args:
            auxData (TxAuxiliaryDataCIP36): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Staking Path or Hash
        data = bytes()
        if auxData.stakingPath.startswith("m/"):
            data += pack_derivation_path(auxData.stakingPath)
        else:
            data = bytes.fromhex(auxData.stakingPath)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_AUX_DATA, P2Type.P2_STAKING_KEY, data)


    def sign_tx_aux_data_payment(self, auxData: TxAuxiliaryDataCIP36) -> bytes:
        """APDU Builder for Sign TX - AUX DATA step - PAYMENT mode

        Args:
            auxData (TxAuxiliaryDataCIP36): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Payment destination
        data = self._serializeTxOutputDestination(auxData.paymentDestination)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_AUX_DATA, P2Type.P2_PAYMENT_ADDRESS, data)


    def sign_tx_aux_data_nonce(self, auxData: TxAuxiliaryDataCIP36) -> bytes:
        """APDU Builder for Sign TX - AUX DATA step - NONCE mode

        Args:
            auxData (TxAuxiliaryDataCIP36): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Nonce (8B)
        data = auxData.nonce.to_bytes(8, "big")
        return self._serialize(InsType.SIGN_TX, P1Type.P1_AUX_DATA, P2Type.P2_NONCE, data)


    def sign_tx_aux_data_voting_purpose(self, auxData: TxAuxiliaryDataCIP36) -> bytes:
        """APDU Builder for Sign TX - AUX DATA step - VOTING PURPOSE mode

        Args:
            auxData (TxAuxiliaryDataCIP36): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Voting Purpose option flag (1B)
        #    Voting Purpose
        data = bytes()
        data += self._serializeOptionFlags(auxData.votingPurpose is not None)
        if auxData.votingPurpose is not None:
            data += auxData.votingPurpose.to_bytes(8, "big")
        return self._serialize(InsType.SIGN_TX, P1Type.P1_AUX_DATA, P2Type.P2_VOTING_PURPOSE, data)


    def sign_tx_aux_data_confirm(self) -> bytes:
        """APDU Builder for Sign TX - AUX DATA step - CONFIRM mode

        Returns:
            Serial data APDU
        """

        return self._serialize(InsType.SIGN_TX, P1Type.P1_AUX_DATA, P2Type.P2_AUX_CONFIRM)


    def sign_tx_inputs(self, txInput: TxInput) -> bytes:
        """APDU Builder for Sign TX - INPUTS step

        Args:
            txInput (TxInput): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Tx Hash Hex
        #    Tx Output Index (4B)
        data = bytes()
        data += bytes.fromhex(txInput.txHashHex)
        data += txInput.outputIndex.to_bytes(4, "big")
        return self._serialize(InsType.SIGN_TX, P1Type.P1_INPUTS, 0x00, data)


    def sign_tx_outputs_basic(self, txOutput: TxOutput) -> bytes:
        """APDU Builder for Sign TX - OUTPUTS step - BASIC DATA level

        Args:
            txOutput (TxOutput): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Format (1B)
        #    Tx Output destination
        #    Coin (8B)
        #    TokenBundle Length (4B)
        #    datum option flag (1B)
        #    referenceScriptHex option flag (1B)
        data = bytes()
        data += txOutput.format.to_bytes(1, "big")
        data += self._serializeTxOutputDestination(txOutput.destination)
        data += self._serializeCoin(txOutput.amount)
        data += len(txOutput.tokenBundle).to_bytes(4, "big")
        data += self._serializeOptionFlags(txOutput.datum is not None)
        if isinstance(txOutput, TxOutputBabbage):
            data += self._serializeOptionFlags(txOutput.referenceScriptHex is not None)
        else:
            data += self._serializeOptionFlags(False)

        return self._serialize(InsType.SIGN_TX, P1Type.P1_OUTPUTS, P2Type.P2_BASIC_DATA, data)


    def sign_tx_outputs_datum(self, datum: Datum) -> bytes:
        """APDU Builder for Sign TX - OUTPUTS step - DATUM level

        Args:
            txInput (TxInput): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Type (1B)
        #    Datum 1st Chunk
        data = bytes()
        data += datum.type.to_bytes(1, "big")
        if datum.type == DatumType.INLINE:
            data += self._serializeTxChunk(datum.datumHex)
        else:
            data += bytes.fromhex(datum.datumHex)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_OUTPUTS, P2Type.P2_DATUM, data)


    def sign_tx_outputs_ref_script(self, referenceScriptHex: str) -> bytes:
        """APDU Sign TX - OUTPUTS step - REFERENCE SCRIPT level

        Args:
            referenceScriptHex (str): Test parameters

        Returns:
            Response APDU
        """

        #    Reference Script Chunk
        data = self._serializeTxChunk(referenceScriptHex)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_OUTPUTS, P2Type.P2_SCRIPT, data)


    def sign_tx_outputs_chunk(self, p2: P2Type, chunkHex: str) -> bytes:
        """APDU Sign TX - OUTPUTS step - xxx CHUNKS level

        Args:
            p2 (P2Type): APDU Parameter 2
            chunkHex (str): Test parameters

        Returns:
            Response APDU
        """

        #    Script Chunk
        length = len(chunkHex) // 2
        data = bytes()
        data += length.to_bytes(4, "big")
        data += bytes.fromhex(chunkHex)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_OUTPUTS, p2, data)


    def sign_tx_outputs_confirm(self) -> bytes:
        """APDU Builder for Sign TX - OUTPUTS step - CONFIRM level

        Returns:
            Serial data APDU
        """

        return self._serialize(InsType.SIGN_TX, P1Type.P1_OUTPUTS, P2Type.P2_CONFIRM)


    def sign_tx_fee(self, testCase: SignTxTestCase) -> bytes:
        """APDU Builder for Sign TX - FEE step

        Args:
            testCase (SignTxTestCase): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Fee (8B)
        data = self._serializeCoin(testCase.tx.fee)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_FEE, 0x00, data)


    def sign_tx_ttl(self, testCase: SignTxTestCase) -> bytes:
        """APDU Builder for Sign TX - TTL step

        Args:
            testCase (SignTxTestCase): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    TTL (8B)
        assert testCase.tx.ttl is not None
        data = self._serializeCoin(testCase.tx.ttl)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_TTL, 0x00, data)


    def sign_tx_withdrawal(self, withdrawal: Withdrawal) -> bytes:
        """APDU Builder for Sign TX - WITHDRAWAL step

        Args:
            withdrawal (Withdrawal): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Amount (8B)
        #    Staking cresentials
        data = bytes()
        data += self._serializeCoin(withdrawal.amount)
        data += self._serializeCredential(withdrawal.stakeCredential)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_WITHDRAWALS, 0x00, data)


    def sign_tx_validity(self, validity: int) -> bytes:
        """APDU Builder for Sign TX - VALIDITY START step

        Args:
            validity (int): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Validity Start (8B)
        data = validity.to_bytes(8, "big")
        return self._serialize(InsType.SIGN_TX, P1Type.P1_VALIDITY_INTERVAL_START, 0x00, data)


    def sign_tx_mint_init(self, nbMints: int) -> bytes:
        """APDU Builder for Sign TX - MINT step - INIT mode

        Args:
            nbMints (int): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Nb of mint elements (4B)
        data = nbMints.to_bytes(4, "big")
        return self._serialize(InsType.SIGN_TX, P1Type.P1_MINT, P2Type.P2_BASIC_DATA, data)


    def sign_tx_mint_confirm(self) -> bytes:
        """APDU Builder for Sign TX - MINT step - CONFIRM mode

        Returns:
            Serial data APDU
        """

        return self._serialize(InsType.SIGN_TX, P1Type.P1_MINT, P2Type.P2_CONFIRM)


    def sign_tx_asset_group(self, p1: P1Type, asset: AssetGroup) -> bytes:
        """APDU Builder for Sign TX - TOKEN BUNDLE step - ASSET mode

        Args:
            p1 (P1Type): APDU Parameter 1
            asset (AssetGroup): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Asset Group
        data = self._serializeAssetGroup(asset)
        return self._serialize(InsType.SIGN_TX, p1, P2Type.ASSET_GROUP, data)


    def sign_tx_token(self, p1: P1Type, token: Token) -> bytes:
        """APDU Builder for Sign TX - TOKEN BUNDLE step - TOKEN mode

        Args:
            p1 (P1Type): APDU Parameter 1
            asset (AssetGroup): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Asset Token
        data = self._serializeToken(token)
        return self._serialize(InsType.SIGN_TX, p1, P2Type.TOKEN, data)


    def sign_tx_script_data_hash(self, script: str) -> bytes:
        """APDU Builder for Sign TX - SCRIPT DATA HASH step

        Args:
            script (str): Input Test script data hash

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Script Data Hash
        data = bytes.fromhex(script)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_SCRIPT_DATA_HASH, 0x00, data)


    def sign_tx_collateral_inputs(self, txInput: TxInput) -> bytes:
        """APDU Builder for Sign TX - COLLATERAL INPUTS step

        Args:
            txInput (TxInput): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Collateral Input
        data = self._serializeTxInput(txInput)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_COLLATERAL_INPUTS, 0x00, data)


    def sign_tx_total_collateral(self, total: int) -> bytes:
        """APDU Builder for Sign TX - TOTAL COLLATERAL step

        Args:
            total (int): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Nb of collateral elements (8B)
        data = self._serializeCoin(total)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_TOTAL_COLLATERAL, 0x00, data)


    def sign_tx_reference_inputs(self, txInput: TxInput) -> bytes:
        """APDU Builder for Sign TX - REFERENCE INPUTS step

        Args:
            txInput (TxInput): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Reference Input
        data = self._serializeTxInput(txInput)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_REFERENCE_INPUTS, 0x00, data)


    def sign_tx_collateral_output_basic(self, txOutput: TxOutput) -> bytes:
        """APDU Builder for Sign TX - COLLATERAL OUTPUTS step - BASIC DATA level

        Args:
            txOutput (TxOutput): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Format (1B)
        #    Tx Output destination
        #    Coin (8B)
        #    TokenBundle Length (4B)
        #    datum option flag (1B)
        #    referenceScriptHex option flag (1B)
        data = bytes()
        data += txOutput.format.to_bytes(1, "big")
        data += self._serializeTxOutputDestination(txOutput.destination)
        data += self._serializeCoin(txOutput.amount)
        data += len(txOutput.tokenBundle).to_bytes(4, "big")
        data += self._serializeOptionFlags(txOutput.datum is not None)
        if isinstance(txOutput, TxOutputBabbage):
            data += self._serializeOptionFlags(txOutput.referenceScriptHex is not None)
        else:
            data += self._serializeOptionFlags(False)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_COLLATERAL_OUTPUT, P2Type.P2_BASIC_DATA, data)


    def sign_tx_collateral_output_confirm(self) -> bytes:
        """APDU Builder for Sign TX - COLLATERAL OUTPUTS step - CONFIRM level

        Returns:
            Serial data APDU
        """

        return self._serialize(InsType.SIGN_TX, P1Type.P1_COLLATERAL_OUTPUT, P2Type.P2_CONFIRM)


    def sign_tx_required_signers(self, signer: RequiredSigner) -> bytes:
        """APDU Builder for Sign TX - REQUIRED SIGNERS step

        Args:
            signer (RequiredSigner): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Type (1B)
        #    Address
        data = signer.type.to_bytes(1, "big")
        if signer.addressHex.startswith("m/"):
            data += pack_derivation_path(signer.addressHex)
        else:
            data += bytes.fromhex(signer.addressHex)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_REQUIRED_SIGNERS, 0x00, data)


    def sign_tx_treasury(self, treasury: int) -> bytes:
        """APDU Builder for Sign TX - TREASURY step

        Args:
            treasury (int): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Coin (8B)
        data = self._serializeCoin(treasury)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_TREASURY, 0x00, data)


    def sign_tx_donation(self, donation: int) -> bytes:
        """APDU Builder for Sign TX - DONATION step

        Args:
            donation (int): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Coin (8B)
        data = self._serializeCoin(donation)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_DONATION, 0x00, data)


    def sign_tx_voting_procedure(self, votingProcedure: VoterVotes) -> bytes:
        """APDU Builder for Sign TX - VOTING PROCEDURES step

        Args:
            votingProcedure (VoterVotes): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Voter Type (1B)
        #    Voter Key
        #    Gov Action Tx Hash
        #    Gov Action Index (4B)
        #    Vote (1B)
        #    Anchor
        assert len(votingProcedure.votes) == 1
        data = bytes()
        data += votingProcedure.voter.type.to_bytes(1, "big")
        if votingProcedure.voter.keyValue.startswith("m/"):
            data += pack_derivation_path(votingProcedure.voter.keyValue)
        else:
            data += bytes.fromhex(votingProcedure.voter.keyValue)
        data += bytes.fromhex(votingProcedure.votes[0].govActionId.txHashHex)
        data += votingProcedure.votes[0].govActionId.govActionIndex.to_bytes(4, "big")
        data += votingProcedure.votes[0].votingProcedure.vote.to_bytes(1, "big")
        data += self._serializeAnchor(votingProcedure.votes[0].votingProcedure.anchor)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_VOTING_PROCEDURES, 0x00, data)


    def sign_tx_certificate(self, certificate: Certificate) -> bytes:
        """APDU Builder for Sign TX - WITHDRAWAL step

        Args:
            certificate (Certificate): Test parameters

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #   Certificate Type (1B)
        #   Certificate Data
        data = bytes()
        if certificate.type in (CertificateType.STAKE_REGISTRATION, CertificateType.STAKE_DEREGISTRATION):
            assert isinstance(certificate.params, StakeRegistrationParams)
            data += certificate.type.to_bytes(1, "big")
            assert certificate.params.stakeCredential is not None
            data += self._serializeCredential(certificate.params.stakeCredential)
        elif certificate.type in (CertificateType.STAKE_REGISTRATION_CONWAY, CertificateType.STAKE_DEREGISTRATION_CONWAY):
            assert isinstance(certificate.params, StakeRegistrationConwayParams)
            data += certificate.type.to_bytes(1, "big")
            assert certificate.params.stakeCredential is not None
            data += self._serializeCredential(certificate.params.stakeCredential)
            assert certificate.params.deposit is not None
            data += self._serializeCoin(certificate.params.deposit)
        elif certificate.type == CertificateType.STAKE_DELEGATION:
            assert isinstance(certificate.params, StakeDelegationParams)
            data += certificate.type.to_bytes(1, "big")
            assert certificate.params.stakeCredential is not None
            data += self._serializeCredential(certificate.params.stakeCredential)
            assert certificate.params.poolKeyHash is not None
            data += bytes.fromhex(certificate.params.poolKeyHash)
        elif certificate.type == CertificateType.VOTE_DELEGATION:
            assert isinstance(certificate.params, VoteDelegationParams)
            data += certificate.type.to_bytes(1, "big")
            assert certificate.params.stakeCredential is not None
            data += self._serializeCredential(certificate.params.stakeCredential)
            assert certificate.params.dRep is not None
            data += self._serializeDRep(certificate.params.dRep)
        elif certificate.type == CertificateType.AUTHORIZE_COMMITTEE_HOT:
            assert isinstance(certificate.params, AuthorizeCommitteeParams)
            data += certificate.type.to_bytes(1, "big")
            assert certificate.params.coldCredential is not None
            data += self._serializeCredential(certificate.params.coldCredential)
            assert certificate.params.hotCredential is not None
            data += self._serializeCredential(certificate.params.hotCredential)
        elif certificate.type == CertificateType.RESIGN_COMMITTEE_COLD:
            assert isinstance(certificate.params, ResignCommitteeParams)
            data += certificate.type.to_bytes(1, "big")
            assert certificate.params.coldCredential is not None
            data += self._serializeCredential(certificate.params.coldCredential)
            data += self._serializeAnchor(certificate.params.anchor)
        elif certificate.type == CertificateType.DREP_REGISTRATION:
            assert isinstance(certificate.params, DRepRegistrationParams)
            data += certificate.type.to_bytes(1, "big")
            assert certificate.params.dRepCredential is not None
            data += self._serializeCredential(certificate.params.dRepCredential)
            assert certificate.params.deposit is not None
            data += self._serializeCoin(certificate.params.deposit)
            data += self._serializeAnchor(certificate.params.anchor)
        elif certificate.type == CertificateType.DREP_DEREGISTRATION:
            assert isinstance(certificate.params, DRepRegistrationParams)
            data += certificate.type.to_bytes(1, "big")
            assert certificate.params.dRepCredential is not None
            data += self._serializeCredential(certificate.params.dRepCredential)
            assert certificate.params.deposit is not None
            data += self._serializeCoin(certificate.params.deposit)
        elif certificate.type == CertificateType.DREP_UPDATE:
            assert isinstance(certificate.params, DRepUpdateParams)
            data += certificate.type.to_bytes(1, "big")
            assert certificate.params.dRepCredential is not None
            data += self._serializeCredential(certificate.params.dRepCredential)
            data += self._serializeAnchor(certificate.params.anchor)
        elif certificate.type == CertificateType.STAKE_POOL_REGISTRATION:
            data += certificate.type.to_bytes(1, "big")
        elif certificate.type == CertificateType.STAKE_POOL_RETIREMENT:
            assert isinstance(certificate.params, PoolRetirementParams)
            data += certificate.type.to_bytes(1, "big")
            assert certificate.params.poolCredential is not None
            data += self._serializeCredential(certificate.params.poolCredential)
            assert certificate.params.retirementEpoch is not None
            data += certificate.params.retirementEpoch.to_bytes(8, "big")
        else:
            raise NotImplementedError("Not implemented yet")

        return self._serialize(InsType.SIGN_TX, P1Type.P1_CERTIFICATES, 0x00, data)


    def sign_tx_cert_pool_reg_init(self, pool: PoolRegistrationParams) -> bytes:
        """APDU Builder for Sign TX - CERTIFICATE step - POOL INITIAL PARAMS level

        Args:
            pool (PoolRegistrationParams): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Pool Owners length (4B)
        #    Pool Relays length (4B)
        data = bytes()
        data += len(pool.poolOwners).to_bytes(4, "big")
        data += len(pool.relays).to_bytes(4, "big")
        return self._serialize(InsType.SIGN_TX, P1Type.P1_CERTIFICATES, P2Type.P2_CERT_INIT, data)


    def sign_tx_cert_pool_reg_pool_key(self, pool: PoolKey) -> bytes:
        """APDU Builder for Sign TX - CERTIFICATE step - POOL KEY level

        Args:
            pool (PoolKey): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Pool Key type (1B)
        #    Pool Key
        data = bytes()
        data += pool.type.to_bytes(1, "big")
        if pool.key.startswith("m/"):
            data += pack_derivation_path(pool.key)
        else:
            data += bytes.fromhex(pool.key)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_CERTIFICATES, P2Type.P2_POOL_KEY, data)


    def sign_tx_cert_pool_reg_vrf(self, pool: str) -> bytes:
        """APDU Builder for Sign TX - CERTIFICATE step - VRF level

        Args:
            pool (str): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    VRF Key
        data = bytes.fromhex(pool)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_CERTIFICATES, P2Type.P2_VRF_KEY, data)


    def sign_tx_cert_pool_reg_financials(self, pool: PoolRegistrationParams) -> bytes:
        """APDU Builder for Sign TX - CERTIFICATE step - FINANCIALS level

        Args:
            pool (PoolRegistrationParams): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Coin pledge (8B)
        #    Coin cost (8B)
        #    Pool margin (8B each)
        data = bytes()
        data += self._serializeCoin(pool.pledge)
        data += self._serializeCoin(pool.cost)
        data += pool.margin.numerator.to_bytes(8, "big")
        data += pool.margin.denominator.to_bytes(8, "big")
        return self._serialize(InsType.SIGN_TX, P1Type.P1_CERTIFICATES, P2Type.P2_FINANCIALS, data)


    def sign_tx_cert_pool_reg_reward(self, pool: PoolKey) -> bytes:
        """APDU Builder for Sign TX - CERTIFICATE step - REWARD ACCOUNT level

        Args:
            pool (PoolKey): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Pool Key type (1B)
        #    Pool Key
        data = bytes()
        data += pool.type.to_bytes(1, "big")
        if pool.key.startswith("m/"):
            data += pack_derivation_path(pool.key)
        else:
            data += bytes.fromhex(pool.key)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_CERTIFICATES, P2Type.P2_REWARD_ACCOUNT, data)


    def sign_tx_cert_pool_reg_owner(self, pool: PoolKey) -> bytes:
        """APDU Builder for Sign TX - CERTIFICATE step - POOL OWNER level

        Args:
            pool (PoolKey): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Pool Key type (1B)
        #    Pool Key
        data = bytes()
        data += pool.type.to_bytes(1, "big")
        if pool.key.startswith("m/"):
            data += pack_derivation_path(pool.key)
        else:
            data += bytes.fromhex(pool.key)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_CERTIFICATES, P2Type.P2_OWNERS, data)


    def sign_tx_cert_pool_reg_relay(self, pool: Relay) -> bytes:
        """APDU Builder for Sign TX - CERTIFICATE step - POOL RELAY level

        Args:
            pool (Relay): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Relay Type (1B)
        #    Relay Data
        data = bytes()
        data += pool.type.to_bytes(1, "big")
        if pool.type == RelayType.SINGLE_HOST_IP_ADDR:
            assert isinstance(pool.params, SingleHostIpAddrRelayParams)
            data += self._serializeOptionFlags(pool.params.portNumber is not None)
            if pool.params.portNumber is not None:
                data += pool.params.portNumber.to_bytes(2, "big")
            data += self._serializeOptionFlags(pool.params.ipv4 is not None)
            if pool.params.ipv4 is not None:
                for ip in pool.params.ipv4.split("."):
                    data += int(ip).to_bytes(1, "big")
            data += self._serializeOptionFlags(pool.params.ipv6 is not None)
            if pool.params.ipv6 is not None:
                ip = pool.params.ipv6.replace(":", "")
                data += bytes.fromhex(ip)

        elif pool.type == RelayType.SINGLE_HOST_HOSTNAME:
            assert isinstance(pool.params, SingleHostHostnameRelayParams)
            data += self._serializeOptionFlags(pool.params.portNumber is not None)
            if pool.params.portNumber is not None:
                data += pool.params.portNumber.to_bytes(2, "big")
            data += pool.params.dnsName.encode("ascii")
        elif pool.type == RelayType.MULTI_HOST:
            assert isinstance(pool.params, MultiHostRelayParams)
            data += pool.params.dnsName.encode("ascii")

        return self._serialize(InsType.SIGN_TX, P1Type.P1_CERTIFICATES, P2Type.P2_RELAYS, data)


    def sign_tx_cert_pool_reg_metadata(self, pool: PoolMetadataParams) -> bytes:
        """APDU Builder for Sign TX - CERTIFICATE step - POOL METADATA level

        Args:
            pool (PoolMetadataParams): Input Test data

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Sign TX Included flag (1B)
        #    Metadata hash
        #    Metadata URL
        data = bytes()
        data += self._serializeOptionFlags(pool is not None)
        if pool is not None:
            data += bytes.fromhex(pool.metadataHashHex)
            data += pool.metadataUrl.encode("ascii")
        return self._serialize(InsType.SIGN_TX, P1Type.P1_CERTIFICATES, P2Type.P2_METADATA, data)


    def sign_tx_cert_pool_reg_confirm(self) -> bytes:
        """APDU Builder for Sign TX - CERTIFICATE step - CONFIRM level

        Returns:
            Serial data APDU
        """

        return self._serialize(InsType.SIGN_TX, P1Type.P1_CERTIFICATES, P2Type.P2_CERT_CONFIRM)


    def sign_tx_confirm(self) -> bytes:
        """APDU Builder for Sign TX - CONFIRM step

        Returns:
            Serial data APDU
        """

        return self._serialize(InsType.SIGN_TX, P1Type.P1_TX_CONFIRM)


    def sign_tx_witness(self, path: str) -> bytes:
        """APDU Builder for Sign TX - WITNESS step

        Args:
            path (str): Input Test path

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Witness Path
        data = pack_derivation_path(path)
        return self._serialize(InsType.SIGN_TX, P1Type.P1_TX_WITNESSES, 0x00, data)


    def derive_script_add_simple(self, script: NativeScript) -> bytes:
        """APDU Builder for DERIVE NATIVE SCRIPT HASH - SIMPLE SCRIPT step

        Args:
            script (NativeScript): Input Test params

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Script type (1B)
        #    Script Pubkey type if PUBKEY_XXX (1B)
        #    Script path (if PUBKEY_XXX)
        #    Script slot (if INVALID_XXX)
        data = bytes()
        scriptType = 0 if script.type == NativeScriptType.PUBKEY_THIRD_PARTY else script.type
        data += scriptType.to_bytes(1, "big")
        if script.type in (NativeScriptType.PUBKEY_DEVICE_OWNED, NativeScriptType.PUBKEY_THIRD_PARTY):
            assert isinstance(script.params, NativeScriptParamsPubkey)
            data += self._derive_script_pubkey(script.type)
            if script.params.key.startswith("m/"):
                data += pack_derivation_path(script.params.key)
            else:
                data += bytes.fromhex(script.params.key)
        elif script.type in (NativeScriptType.INVALID_BEFORE, NativeScriptType.INVALID_HEREAFTER):
            assert isinstance(script.params, NativeScriptParamsInvalid)
            data += script.params.slot.to_bytes(8, "big")
        return self._serialize(InsType.DERIVE_SCRIPT_HASH, P1Type.P1_ADD_SIMPLE_SCRIPT, 0x00, data)


    def derive_script_add_complex(self, script: NativeScript) -> bytes:
        """APDU Builder for DERIVE NATIVE SCRIPT HASH - COMPLEX SCRIPT step

        Args:
            script (NativeScript): Input Test params

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Script type (1B)
        #    Script Pubkey type if PUBKEY_XXX (1B)
        #    Script path (if PUBKEY_XXX)
        #    Script slot (if INVALID_XXX)
        data = bytes()
        data += script.type.to_bytes(1, "big")
        if script.type in (NativeScriptType.ALL, NativeScriptType.ANY):
            assert isinstance(script.params, NativeScriptParamsScripts)
            data += len(script.params.scripts).to_bytes(4, "big")
        elif script.type == NativeScriptType.N_OF_K:
            assert isinstance(script.params, NativeScriptParamsNofK)
            data += len(script.params.scripts).to_bytes(4, "big")
            data += script.params.requiredCount.to_bytes(4, "big")
        return self._serialize(InsType.DERIVE_SCRIPT_HASH, P1Type.P1_COMPLEX_SCRIPT_START, 0x00, data)


    def derive_script_finish(self, disp: NativeScriptHashDisplayFormat) -> bytes:
        """APDU Builder for DERIVE NATIVE SCRIPT HASH - FINISH step

        Args:
            disp (NativeScriptHashDisplayFormat): Input Test params

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Display format (1B)
        data = disp.to_bytes(1, "big")
        return self._serialize(InsType.DERIVE_SCRIPT_HASH, P1Type.P1_WHOLE_NATIVE_SCRIPT_FINISH, 0x00, data)


    def _derive_script_pubkey(self, scriptType: NativeScriptType) -> bytes:
        """APDU Builder for DERIVE NATIVE SCRIPT HASH - START step

        Args:
            scriptType (NativeScriptType): Input script type

        Returns:
            Serial data APDU
        """

        # Serialization format:
        #    Encoding (1B)
        if scriptType == NativeScriptType.PUBKEY_DEVICE_OWNED:
            encoding = 1
        elif scriptType == NativeScriptType.PUBKEY_THIRD_PARTY:
            encoding = 2
        else:
            encoding = 0

        return encoding.to_bytes(1, "big")


    def _serializeTxChunk(self, referenceHex: str) -> bytes:
        """Serialize TX Chunk"""

        # Serialization format:
        #    Full data length (4B)
        #    Chunk size (4B)
        #    Chunk data
        data = bytes()
        totalSize = len(referenceHex) // 2
        data += totalSize.to_bytes(4, "big")
        if totalSize > MAX_SIGN_TX_CHUNK_SIZE:
            chunkHex = referenceHex[:MAX_SIGN_TX_CHUNK_SIZE * 2]
        else:
            chunkHex = referenceHex
        chunkSize = len(chunkHex) // 2
        data += chunkSize.to_bytes(4, "big")
        data += bytes.fromhex(chunkHex)
        return data


    def _serializeTxInput(self, txInput: TxInput) -> bytes:
        """Serialize TX Input"""

        # Serialization format:
        #    Input Hash
        #    Output Index (4B)
        data = bytes()
        data += bytes.fromhex(txInput.txHashHex)
        data += txInput.outputIndex.to_bytes(4, "big")
        return data


    def _serializeAnchor(self, anchor: Optional[AnchorParams] = None) -> bytes:
        """Serialize Anchor"""

        # Serialization format:
        #    Anchor option flag (1B)
        #    Anchor hash
        #    Anchor URL
        data = bytes()
        data += self._serializeOptionFlags(anchor is not None)
        if anchor is not None:
            data += bytes.fromhex(anchor.hashHex)
            data += anchor.url.encode("ascii")
        return data


    def _serializeOptionFlags(self, included: bool) -> bytes:
        """Serialize Flag option value"""

        # Serialization format:
        #    Flag value (1B): 02 if included, 01 otherwise
        value = 0x02 if included else 0x01
        return value.to_bytes(1, "big")


    def _serializeCoin(self, coin: int) -> bytes:
        """Serialize Coin value"""

        return coin.to_bytes(8, "big")


    def _serializeCredential(self, credential: CredentialParams) -> bytes:
        """Serialize Credential"""

        # Serialization format:
        #    Type (1B)
        #    Credential data
        data = bytes()
        data += credential.type.to_bytes(1, "big")
        assert credential.keyValue is not None
        if credential.keyValue.startswith("m/"):
            data += pack_derivation_path(credential.keyValue)
        else:
            data += bytes.fromhex(credential.keyValue)
        return data


    def _serializeDRep(self, dRep: DRepParams) -> bytes:
        """Serialize DRep"""

        # Serialization format:
        #    Type (1B)
        #    DRep data
        data = bytes()
        data += dRep.type.to_bytes(1, "big")
        if dRep.keyValue is not None:
            if dRep.keyValue.startswith("m/"):
                data += pack_derivation_path(dRep.keyValue)
            else:
                data += bytes.fromhex(dRep.keyValue)
        return data


    def _serializeTxOutputDestination(self, outDest: TxOutputDestination) -> bytes:
        """Serialize TX Output Destination"""

        # Serialization format:
        #    Type (1B)
        #    Destination data
        data = bytes()
        data += outDest.type.to_bytes(1, "big")
        if outDest.type == TxOutputDestinationType.THIRD_PARTY:
            assert isinstance(outDest.params, ThirdPartyAddressParams)
            data += int(len(outDest.params.addressHex) / 2).to_bytes(4, "big")
            data += bytes.fromhex(outDest.params.addressHex)
        else:
            assert isinstance(outDest.params, DeriveAddressTestCase)
            data += self._serializeAddressParams(outDest.params)
        return data


    def _serializeAddressParams(self, testCase: DeriveAddressTestCase) -> bytes:
        """Serialize address parameters"""

        # Serialization format (from the documentation):
        # address type 1B
        # if address type == BYRON
        #     protocol magic 4B
        # else
        #     network id 1B
        # payment public key derivation path (1B for length + [0-10] x 4B) or script hash 28B
        # staking choice 1B
        #     if NO_STAKING:
        #         nothing more
        #     if STAKING_KEY_PATH:
        #         staking public key derivation path (1B for length + [0-10] x 4B)
        #     if STAKING_KEY_HASH:
        #         stake key hash 28B
        #     if BLOCKCHAIN_POINTER:
        #         certificate blockchain pointer 3 x 4B
        data = bytes()
        data += testCase.addrType.to_bytes(1, "big")
        if testCase.addrType == AddressType.BYRON:
            data += testCase.netDesc.protocol.to_bytes(4, "big")
        else:
            data += testCase.netDesc.networkId.to_bytes(1, "big")

        if not testCase.spendingValue.startswith("m/"):
            data += bytes.fromhex(testCase.spendingValue)
        elif testCase.spendingValue:
            data += pack_derivation_path(testCase.spendingValue)

        if testCase.addrType in (AddressType.BYRON, AddressType.ENTERPRISE_KEY,
                        AddressType.ENTERPRISE_SCRIPT):
            staking = StakingDataSourceType.NONE
        elif testCase.addrType in (AddressType.BASE_PAYMENT_KEY_STAKE_SCRIPT,
                          AddressType.BASE_PAYMENT_SCRIPT_STAKE_SCRIPT,
                          AddressType.REWARD_SCRIPT):
            staking = StakingDataSourceType.SCRIPT_HASH
        elif testCase.addrType in (AddressType.POINTER_KEY, AddressType.POINTER_SCRIPT):
            staking = StakingDataSourceType.BLOCKCHAIN_POINTER
        elif not testCase.stakingValue.startswith("m/"):
            staking = StakingDataSourceType.KEY_HASH
        else:
            staking = StakingDataSourceType.KEY_PATH
        data += staking.to_bytes(1, "big")

        if staking == StakingDataSourceType.KEY_PATH:
            data += pack_derivation_path(testCase.stakingValue)
        elif staking in (StakingDataSourceType.KEY_HASH,
                         StakingDataSourceType.SCRIPT_HASH,
                         StakingDataSourceType.BLOCKCHAIN_POINTER):
            data += bytes.fromhex(testCase.stakingValue)
        elif staking != StakingDataSourceType.NONE:
            raise NotImplementedError("Not implemented yet")

        return data


    def _serializeAssetGroup(self, asset: AssetGroup) -> bytes:
        """Serialize Asset Group"""

        # Serialization format:
        #    Policy ID
        #    Nb of tokens (4B)
        data = bytes()
        data += bytes.fromhex(asset.policyIdHex)
        data += len(asset.tokens).to_bytes(4, "big")
        return data


    def _serializeToken(self, token: Token) -> bytes:
        """Serialize Token"""

        # Serialization format:
        #    Asset Name Length (4B)
        #    Asset Name
        #    Amount (8B)
        data = bytes()
        data += int(len(token.assetNameHex) / 2).to_bytes(4, "big")
        data += bytes.fromhex(token.assetNameHex)
        data += token.amount.to_bytes(8, "big", signed=True)
        return data

    def serialize_transaction_chunks(self, tx: Transaction) -> list:
        """Create transaction data chunks for sending to device.

        Breaks the serialized transaction into chunks of MAX_SIGN_TX_CHUNK_SIZE bytes,
        with appropriate APDU headers for each chunk.

        Args:
            tx: Transaction object from signTx.py

        Returns:
            List of APDU bytes, one per chunk
        """
        # Serialize the full transaction first
        tx_data = self._serialize_transaction_unpacked_raw(tx)

        chunks = []
        offset = 0

        while offset < len(tx_data):
            chunk_size = min(MAX_SIGN_TX_CHUNK_SIZE, len(tx_data) - offset)
            chunk_data = tx_data[offset:offset + chunk_size]
            offset += chunk_size

            # Determine if this is the last chunk
            more = offset < len(tx_data)

            # Create APDU for this chunk
            p1 = P1Type.P1_TX_DATA_CHUNK if more else P1Type.P1_TX_CHUNK_LAST
            chunk_apdu = self._serialize(InsType.SIGN_TX, p1, P1Type.P2_UNUSED, chunk_data)
            chunks.append(chunk_apdu)

        return chunks

    def _serialize_transaction_unpacked_raw(self, tx: Transaction) -> bytes:
        """Serialize transaction to unpacked binary format for handler_sign_tx.

        Transaction data buffer (sent after INIT APDU):
        - For each input:
            - tx_hash (32 bytes)
            - output_index (uint32, BE)
        - For each output:
            - output_length (uint16, BE)
            - destination_type (uint8): 0x01=THIRD_PARTY, 0x02=DEVICE_OWNED
            - If THIRD_PARTY:
                - address_size (uint16, BE)
                - address_bytes
            - If DEVICE_OWNED:
                - path_length (uint8)
                - bip32_path
            - ada_amount (uint64, BE)
            - output_format (uint8: 0=ARRAY_LEGACY, 1=MAP_BABBAGE)
            - num_asset_groups (uint16, BE)
            - [asset groups...]
            - datum_flag (uint8)
            - [datum data...]
            - reference_script_flag (uint8)
            - [reference script data...]
        - fee (uint64, BE)
        - ttl (uint64, BE) - only if tx.ttl is not None
        - validity_interval_start (uint64, BE) - only if tx.validityIntervalStart is not None
        - [withdrawals...] - num_withdrawals times (counts from INIT APDU)
        - [certificates...] - until buffer exhausted (counts from INIT APDU)

        Note: num_inputs, num_outputs, num_withdrawals, and field inclusion flags are sent in the INIT APDU

        Args:
            tx: Transaction object from signTx.py

        Returns:
            bytes: Serialized transaction
        """
        from ragger.bip import pack_derivation_path

        data = bytearray()

        # Inputs (no count prefix - sent in INIT APDU)
        for tx_input in tx.inputs:
            data.extend(bytes.fromhex(tx_input.txHashHex))
            data.extend(tx_input.outputIndex.to_bytes(4, 'big'))

        # Outputs (no count prefix - sent in INIT APDU)
        for output_idx, tx_output in enumerate(tx.outputs):
            # Build output data
            output_data = bytearray()
            output_data.append(tx_output.destination.type)

            if tx_output.destination.type == TxOutputDestinationType.THIRD_PARTY:
                addr_bytes = bytes.fromhex(tx_output.destination.params.addressHex)
                output_data.extend(len(addr_bytes).to_bytes(2, 'big'))
                output_data.extend(addr_bytes)
            elif tx_output.destination.type == TxOutputDestinationType.DEVICE_OWNED:
                # Serialize device-owned destination in the format expected by handler:
                # [address_type][protocol_magic (Byron only)][payment_info][staking_choice][staking_info]
                # Note: For Shelley addresses, networkId comes from the tx init, not from output data
                addr_params = tx_output.destination.params

                # Address type (1B)
                output_data.append(addr_params.addrType)

                # Protocol Magic only for Byron addresses (4B)
                # For Shelley addresses, network ID is taken from tx init, not serialized here
                if addr_params.addrType == AddressType.BYRON:
                    output_data.extend(addr_params.netDesc.protocol.to_bytes(4, 'big'))

                # Payment credential (path or script hash)
                if addr_params.spendingValue.startswith("m/"):
                    # Payment key path
                    output_data.extend(pack_derivation_path(addr_params.spendingValue))
                else:
                    # Payment script hash (28 bytes, no length prefix)
                    output_data.extend(bytes.fromhex(addr_params.spendingValue))

                # Staking choice (1B)
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

                # Staking credential based on staking choice
                if staking_choice == StakingDataSourceType.KEY_PATH:
                    output_data.extend(pack_derivation_path(addr_params.stakingValue))
                elif staking_choice in (StakingDataSourceType.KEY_HASH,
                                       StakingDataSourceType.SCRIPT_HASH):
                    output_data.extend(bytes.fromhex(addr_params.stakingValue))
                elif staking_choice == StakingDataSourceType.BLOCKCHAIN_POINTER:
                    # Blockchain pointer: 3 x uint32 (12 bytes total)
                    output_data.extend(bytes.fromhex(addr_params.stakingValue))
                # else NO_STAKING: no additional data

            # ADA amount
            output_data.extend(tx_output.amount.to_bytes(8, 'big'))

            # Output format (uint8: 0=ARRAY_LEGACY, 1=MAP_BABBAGE)
            output_data.append(tx_output.format if hasattr(tx_output, 'format') else 0)

            # Number of asset groups (uint16, BE)
            num_asset_groups = len(tx_output.tokenBundle) if hasattr(tx_output, 'tokenBundle') else 0
            output_data.extend(num_asset_groups.to_bytes(2, 'big'))

            # Asset groups (if any)
            if num_asset_groups > 0:
                for asset_group in tx_output.tokenBundle:
                    # Policy ID (28 bytes, no length prefix)
                    output_data.extend(bytes.fromhex(asset_group.policyIdHex))
                    # Number of tokens (uint16, BE)
                    output_data.extend(len(asset_group.tokens).to_bytes(2, 'big'))
                    # Tokens
                    for token in asset_group.tokens:
                        # Asset name length (uint8)
                        asset_name_bytes = bytes.fromhex(token.assetNameHex)
                        output_data.append(len(asset_name_bytes))
                        # Asset name
                        output_data.extend(asset_name_bytes)
                        # Token amount (int64, BE, signed)
                        output_data.extend(token.amount.to_bytes(8, 'big', signed=True))

            # Datum (if any)
            # Wire format: 0=NONE, 1=HASH (32 bytes, no length), 2=INLINE (u16 length + data)
            if hasattr(tx_output, 'datum') and tx_output.datum is not None:
                datum_type = tx_output.datum.type
                if datum_type == 0:  # HASH
                    output_data.append(0x01)
                    datum_bytes = bytes.fromhex(tx_output.datum.datumHex)
                    output_data.extend(datum_bytes)
                elif datum_type == 1:  # INLINE
                    output_data.append(0x02)
                    datum_bytes = bytes.fromhex(tx_output.datum.datumHex)
                    output_data.extend(len(datum_bytes).to_bytes(2, 'big'))
                    output_data.extend(datum_bytes)
            else:
                output_data.append(0x00)

            # Reference script (if any, for Babbage format)
            if isinstance(tx_output, TxOutputBabbage) and tx_output.referenceScriptHex is not None:
                output_data.append(0x02)
                script_bytes = bytes.fromhex(tx_output.referenceScriptHex)
                output_data.extend(len(script_bytes).to_bytes(2, 'big'))
                output_data.extend(script_bytes)
            else:
                output_data.append(0x00)

            # Add output with length prefix
            data.extend(len(output_data).to_bytes(2, 'big'))
            data.extend(output_data)

        # Fee
        data.extend(tx.fee.to_bytes(8, 'big'))

        # TTL (optional - only if present in transaction)
        # Validation: TTL serialization must stay in sync with include_ttl flag from INIT APDU
        # If include_ttl=True in INIT, tx.ttl must be not None; if False, tx.ttl must be None
        if tx.ttl is not None:
            data.extend(tx.ttl.to_bytes(8, 'big'))

        # Certificates (num_certificates is sent in INIT APDU)
        for certificate in tx.certificates:
            # Certificate type (uint8)
            data.append(certificate.type)

            # Certificate data based on type
            if certificate.type in (CertificateType.STAKE_REGISTRATION, CertificateType.STAKE_DEREGISTRATION):
                assert isinstance(certificate.params, StakeRegistrationParams)
                assert certificate.params.stakeCredential is not None
                # Credential type and data
                cred = certificate.params.stakeCredential
                if cred.type == CredentialParamsType.KEY_PATH:
                    data.append(0x22)  # STAKING_KEY_PATH
                    data.extend(pack_derivation_path(cred.keyValue))
                elif cred.type == CredentialParamsType.KEY_HASH:
                    data.append(0x33)  # STAKING_KEY_HASH
                    data.extend(bytes.fromhex(cred.keyValue))
                elif cred.type == CredentialParamsType.SCRIPT_HASH:
                    data.append(0x55)  # STAKING_SCRIPT_HASH
                    data.extend(bytes.fromhex(cred.keyValue))

            elif certificate.type in (CertificateType.STAKE_REGISTRATION_CONWAY, CertificateType.STAKE_DEREGISTRATION_CONWAY):
                assert isinstance(certificate.params, StakeRegistrationConwayParams)
                assert certificate.params.stakeCredential is not None
                # Credential type and data
                cred = certificate.params.stakeCredential
                if cred.type == CredentialParamsType.KEY_PATH:
                    data.append(0x22)  # STAKING_KEY_PATH
                    data.extend(pack_derivation_path(cred.keyValue))
                elif cred.type == CredentialParamsType.KEY_HASH:
                    data.append(0x33)  # STAKING_KEY_HASH
                    data.extend(bytes.fromhex(cred.keyValue))
                elif cred.type == CredentialParamsType.SCRIPT_HASH:
                    data.append(0x55)  # STAKING_SCRIPT_HASH
                    data.extend(bytes.fromhex(cred.keyValue))
                # Deposit (uint64, BE)
                assert certificate.params.deposit is not None
                data.extend(certificate.params.deposit.to_bytes(8, 'big'))

            elif certificate.type == CertificateType.STAKE_DELEGATION:
                assert isinstance(certificate.params, StakeDelegationParams)
                assert certificate.params.stakeCredential is not None
                # Credential type and data
                cred = certificate.params.stakeCredential
                if cred.type == CredentialParamsType.KEY_PATH:
                    data.append(0x22)  # STAKING_KEY_PATH
                    data.extend(pack_derivation_path(cred.keyValue))
                elif cred.type == CredentialParamsType.KEY_HASH:
                    data.append(0x33)  # STAKING_KEY_HASH
                    data.extend(bytes.fromhex(cred.keyValue))
                elif cred.type == CredentialParamsType.SCRIPT_HASH:
                    data.append(0x55)  # STAKING_SCRIPT_HASH
                    data.extend(bytes.fromhex(cred.keyValue))
                # Pool key hash (28 bytes)
                assert certificate.params.poolKeyHash is not None
                data.extend(bytes.fromhex(certificate.params.poolKeyHash))

            elif certificate.type == CertificateType.STAKE_POOL_RETIREMENT:
                assert isinstance(certificate.params, PoolRetirementParams)
                assert certificate.params.poolCredential is not None
                data += self._serializeCredential(certificate.params.poolCredential)
                # Retirement epoch (uint64, BE)
                assert certificate.params.retirementEpoch is not None
                data.extend(certificate.params.retirementEpoch.to_bytes(8, 'big'))

            elif certificate.type == CertificateType.VOTE_DELEGATION:
                assert isinstance(certificate.params, VoteDelegationParams)
                assert certificate.params.stakeCredential is not None
                # Stake credential type and data
                cred = certificate.params.stakeCredential
                if cred.type == CredentialParamsType.KEY_PATH:
                    data.append(0x22)  # STAKING_KEY_PATH
                    data.extend(pack_derivation_path(cred.keyValue))
                elif cred.type == CredentialParamsType.KEY_HASH:
                    data.append(0x33)  # STAKING_KEY_HASH
                    data.extend(bytes.fromhex(cred.keyValue))
                elif cred.type == CredentialParamsType.SCRIPT_HASH:
                    data.append(0x55)  # STAKING_SCRIPT_HASH
                    data.extend(bytes.fromhex(cred.keyValue))
                # DRep (handled by _serializeDRep logic)
                assert certificate.params.dRep is not None
                drep = certificate.params.dRep
                # DRep type and data would go here
                # For now, basic support

            # Other certificate types can be added as needed

        # Validity Interval Start (optional - only if present in transaction)
        if tx.validityIntervalStart is not None:
            data.extend(tx.validityIntervalStart.to_bytes(8, 'big'))

        # Mint (num_mint_asset_groups is sent in INIT APDU)
        for mint_asset_group in tx.mint:
            # Policy ID (28 bytes, no length prefix)
            data.extend(bytes.fromhex(mint_asset_group.policyIdHex))

            # Number of tokens (uint16, BE)
            data.extend(len(mint_asset_group.tokens).to_bytes(2, 'big'))

            # Tokens
            for token in mint_asset_group.tokens:
                # Asset name length (uint8)
                asset_name_bytes = bytes.fromhex(token.assetNameHex)
                data.append(len(asset_name_bytes))

                # Asset name
                data.extend(asset_name_bytes)

                # Token amount (int64, BE, signed)
                data.extend(token.amount.to_bytes(8, 'big', signed=True))

        # Withdrawals (num_withdrawals is sent in INIT APDU)
        for withdrawal in tx.withdrawals:
            # Amount (uint64, BE)
            data.extend(withdrawal.amount.to_bytes(8, 'big'))

            # Credential type (uint8) - convert to staking_data_source_t values
            # Python CredentialParamsType: KEY_PATH=0x00, SCRIPT_HASH=0x01, KEY_HASH=0x02
            # C staking_data_source_t: STAKING_KEY_PATH=0x22, STAKING_KEY_HASH=0x33, STAKING_SCRIPT_HASH=0x55
            if withdrawal.stakeCredential.type == 0x00:  # KEY_PATH
                credential_type_wire = 0x22  # STAKING_KEY_PATH
            elif withdrawal.stakeCredential.type == 0x01:  # SCRIPT_HASH
                credential_type_wire = 0x55  # STAKING_SCRIPT_HASH
            elif withdrawal.stakeCredential.type == 0x02:  # KEY_HASH
                credential_type_wire = 0x33  # STAKING_KEY_HASH
            else:
                raise ValueError(f"Unknown credential type: {withdrawal.stakeCredential.type}")

            data.append(credential_type_wire)

            # Credential data based on type
            if withdrawal.stakeCredential.keyValue.startswith("m/"):
                # KEY_PATH: serialize as bip32 path
                data.extend(pack_derivation_path(withdrawal.stakeCredential.keyValue))
            else:
                # KEY_HASH or SCRIPT_HASH: serialize as raw bytes (28 bytes, no length prefix)
                data.extend(bytes.fromhex(withdrawal.stakeCredential.keyValue))

        return bytes(data)
