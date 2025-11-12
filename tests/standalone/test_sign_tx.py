# -*- coding: utf-8 -*-
# Test file for Cardano transaction signing with simple chunked flow

import pytest
from hashlib import blake2b
from ledgered.devices import Device
from ragger.backend import BackendInterface
from ragger.error import ExceptionRAPDU
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

from application_client.app_def import Errors
from application_client.command_sender import CommandSender
from application_client.command_builder import gather_witness_paths
from standalone.utils import verify_signature, idTestFunc
from standalone.input_files.signTx import testsShelleyNoCertificates, SignTxTestCase


@pytest.mark.parametrize(
    "testCase",
    testsShelleyNoCertificates,  # Just test the first case for now
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

    # Gather unique witness paths from transaction elements
    witness_paths = gather_witness_paths(tx, testCase.additionalWitnessPaths)
    assert witness_paths, "No witness paths found in transaction"

    # Calculate expected transaction hash from the CBOR txBody
    expected_cbor = bytes.fromhex(testCase.txBody)
    expected_hash = blake2b(expected_cbor, digest_size=32).digest()
    print(f"Expected tx hash: {expected_hash.hex()}")
    print(f"Witness paths: {witness_paths}")

    # Step 1: Send INIT APDU with transaction description
    response = client.sign_tx_init_simple(
        options=testCase.options,
        tx_signing_mode=testCase.signingMode,
        network_id=tx.network.networkId,
        protocol_magic=tx.network.protocol,
        num_inputs=len(tx.inputs),
        num_outputs=len(tx.outputs),
        include_ttl=tx.ttl is not None,
        num_withdrawals=len(tx.withdrawals),
        include_validity_interval_start=tx.validityIntervalStart is not None,
        num_witnesses=len(witness_paths)
    )
    assert response.status == Errors.SW_SUCCESS, f"Init failed: {hex(response.status)}"

    # Step 2: Send transaction data chunks
    # Deserialization only happens after the final chunk is received
    # If deserialization fails, ExceptionRAPDU will be raised automatically
    with client.sign_tx_serialize_and_send_chunks_async(tx):
        # Navigate while the final chunk is being processed
        if device.is_nano:
            # TODO: Add proper navigation for nano devices
            navigator.navigate_until_text(NavInsID.RIGHT_CLICK, [NavInsID.BOTH_CLICK], "Sign transaction")
        else:
            # Check if test case expects warnings (for now we don't have warnings in simple tests)
            scenario_navigator.review_approve(do_comparison=False)

    # Get the response from the final chunk after navigation
    # The final chunk response contains the transaction hash
    response = client.get_async_response()
    assert response is not None, "No response from final chunk"
    tx_hash = response.data
    print(f"Actual tx hash:   {tx_hash.hex()}")
    assert len(tx_hash) == 32, f"Expected 32-byte tx hash, got {len(tx_hash)}"
    assert tx_hash == expected_hash, f"Transaction hash mismatch!\nExpected: {expected_hash.hex()}\nActual:   {tx_hash.hex()}"

    # Step 4: Get witness signatures
    # After user approval, request signatures for all witness paths
    for path in witness_paths:
        response = client.sign_tx_witness(path)
        assert response.status == Errors.SW_SUCCESS, f"Witness failed for {path}: {hex(response.status)}"

        signature = response.data
        print(f"Witness signature for {path} ({len(signature)} bytes): {signature.hex()}")
        assert len(signature) == 64, f"Expected 64-byte signature for {path}, got {len(signature)}"
        verify_signature(path, signature, tx_hash)
