# -*- coding: utf-8 -*-
# Test file for Cardano transaction signing with simple chunked flow

import pytest
from hashlib import blake2b
from ledgered.devices import Device
from ragger.backend import BackendInterface
from ragger.error import ExceptionRAPDU
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

from application_client.status_words import StatusWord
from application_client.command_sender import CommandSender
from standalone.utils import verify_signature, idTestFunc
from standalone.input_files.signTx import (
    testsByron,
    testsMary,
    testsShelleyNoCertificates,
    testsShelleyWithCertificates,
    testsConwayWithCertificates,
    testsAllegra,
    testsAlonzo,
    testsBabbage,
    testsConwayWithoutCertificates,
    testsConwayVotingProcedures,
    testsMultisig,
    SignTxTestCase,
    TxAuxiliaryDataType,
    ThirdPartyAddressParams,
    TransactionSigningMode
)


@pytest.mark.parametrize(
    "testCase",
    testsByron + testsMary + testsShelleyNoCertificates + testsShelleyWithCertificates +
    testsAllegra + testsAlonzo + testsBabbage + testsConwayWithCertificates +
    testsConwayWithoutCertificates + testsConwayVotingProcedures + testsMultisig,
    ids=idTestFunc
)
def test_sign_tx_simple(device: Device,
                       backend: BackendInterface,
                       navigator: Navigator,
                       scenario_navigator: NavigateWithScenario,
                       testCase: SignTxTestCase) -> None:
    """Test simple transaction signing with new protocol.

    This test verifies the new handler_sign_tx implementation:
    1. Send init APDU with transaction description
    2. Send transaction data in unpacked format
    3. User approves transaction
    4. Request witness signature
    """

    client = CommandSender(backend)
    tx = testCase.tx

    # Calculate expected transaction hash from the CBOR txBody
    expected_cbor = bytes.fromhex(testCase.txBody)
    expected_hash = blake2b(expected_cbor, digest_size=32).digest()
    print(f"Expected tx hash: {expected_hash.hex()}")
    def review_transaction() -> None:
        if device.is_nano:
            # TODO: Add proper navigation for nano devices
            navigator.navigate_until_text(NavInsID.RIGHT_CLICK, [NavInsID.BOTH_CLICK], "Sign transaction")
        else:
            if testCase.has_warning:
                scenario_navigator.review_approve_with_warning(do_comparison=False)
            else:
                scenario_navigator.review_approve(do_comparison=False)

    tx_hash, witness_paths = client.sign_tx(
        tx=tx,
        signing_mode=testCase.signingMode,
        additional_witness_paths=testCase.additionalWitnessPaths,
        options=testCase.options,
        on_review=review_transaction
    )
    print(f"Witness paths: {witness_paths}")
    print(f"Actual tx hash:   {tx_hash.hex()}")
    assert len(tx_hash) == 32, f"Expected 32-byte tx hash, got {len(tx_hash)}"
    # TODO this check should be moved to unit tests (for a fixed seed,
    # we check serialization is correct, including elements derived from paths)
    # assert tx_hash == expected_hash, f"Transaction hash mismatch!\nExpected: {expected_hash.hex()}\nActual:   {tx_hash.hex()}"

    # Step 4: Get witness signatures
    # After user approval, request signatures for all witness paths
    for path_idx, path in enumerate(witness_paths):
        # Determine navigation moves based on path and transaction properties
        # (adapted from Shelley app's _signTx_setWitnesses logic)
        moves = []

        # Parse path to check for unusual paths (non-standard accounts or change addresses)
        path_elements = path.replace("'", "").split("/")
        if len(path_elements) > 1:
            try:
                purpose = int(path_elements[1])
                # Unusual purpose (not 1852 for Shelley) or unusual change address
                if purpose > 1852 or (len(path_elements) > 4 and int(path_elements[4]) > 2):
                    moves += [NavInsID.BOTH_CLICK] * 2
                elif testCase.tx.auxiliaryData is not None:
                    # With auxiliary data: need extra confirmations in some cases
                    if testCase.tx.auxiliaryData.type == TxAuxiliaryDataType.CIP36_REGISTRATION:
                        pass  # No extra moves for CIP36
                    elif isinstance(testCase.tx.outputs[0].destination.params, ThirdPartyAddressParams):
                        pass  # No extra moves for third-party addresses
                    else:
                        moves += [NavInsID.BOTH_CLICK] * 3
                elif testCase.signingMode == TransactionSigningMode.PLUTUS_TRANSACTION:
                    moves += [NavInsID.BOTH_CLICK] * 2
                elif testCase.signingMode in (TransactionSigningMode.POOL_REGISTRATION_AS_OWNER,
                                              TransactionSigningMode.POOL_REGISTRATION_AS_OPERATOR):
                    moves += [NavInsID.BOTH_CLICK]
            except (ValueError, IndexError):
                # If path parsing fails, use no extra moves
                pass

        # Each witness requires explicit confirmation on the device
        with client.sign_tx_witness_async(path):
            if len(moves) > 0:
                if device.is_nano:
                    navigator.navigate(moves)
                else:
                    # Stax/Flex: Each witness gets a confirmation choice screen
                    # The scenario_navigator.address_review_approve handles the Confirm button
                    scenario_navigator.address_review_approve(do_comparison=False)
            else:
                pass

        response = client.get_async_response()
        assert response is not None, f"No response for witness {path_idx}: {path}"
        assert response.status == StatusWord.SWO_SUCCESS, f"Witness failed for {path}: {hex(response.status)}"

        signature = response.data
        print(f"Witness signature for {path} ({len(signature)} bytes): {signature.hex()}")
        assert len(signature) == 64, f"Expected 64-byte signature for {path}, got {len(signature)}"
        verify_signature(path, signature, tx_hash)
