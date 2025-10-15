# -*- coding: utf-8 -*-
# Test file for Cardano transaction signing with simple chunked flow

import pytest
from hashlib import blake2b
from ledgered.devices import Device
from ragger.backend import BackendInterface
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

from application_client.app_def import Errors
from application_client.command_sender import CommandSender
from standalone.utils import verify_signature, idTestFunc
from standalone.input_files.signTx import testsShelleyNoCertificates, SignTxTestCase


@pytest.mark.parametrize(
    "testCase",
    testsShelleyNoCertificates[:1],  # Just test the first case for now
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

    # Witness path from the input
    witness_path = tx.inputs[0].path  # m/1852'/1815'/0'/0/0

    # Calculate expected transaction hash from the CBOR txBody
    expected_cbor = bytes.fromhex(testCase.txBody)
    expected_hash = blake2b(expected_cbor, digest_size=32).digest()
    print(f"Expected tx hash: {expected_hash.hex()}")

    # Step 1: Send INIT APDU with transaction description
    response = client.sign_tx_init_simple(
        options=testCase.options,
        tx_signing_mode=testCase.signingMode,
        network_id=tx.network.networkId,
        protocol_magic=tx.network.protocol,
        num_inputs=len(tx.inputs),
        num_outputs=len(tx.outputs),
        include_ttl=tx.ttl is not None
    )
    assert response.status == Errors.SW_SUCCESS, f"Init failed: {hex(response.status)}"

    # Step 2: Send transaction data and navigate to approve
    # For this small transaction, we can send it in one chunk
    # The last chunk triggers UI display, so we use async exchange for navigation
    with client.sign_tx_serialize_and_send_chunk_async(tx):
        if device.is_nano:
            # TODO: Add proper navigation for nano devices
            navigator.navigate_until_text(NavInsID.RIGHT_CLICK, [NavInsID.BOTH_CLICK], "Sign transaction")
        else:
            # Check if test case expects warnings (for now we don't have warnings in simple tests)
            scenario_navigator.review_approve()

    # Get the response from the last chunk (should contain tx hash)
    response = client.get_async_response()
    assert response and response.status == Errors.SW_SUCCESS, f"Chunk failed: {hex(response.status)}"

    # The last chunk response should contain the transaction hash
    tx_hash = response.data
    print(f"Actual tx hash:   {tx_hash.hex()}")
    assert len(tx_hash) == 32, f"Expected 32-byte tx hash, got {len(tx_hash)}"

    # Verify the hash matches the expected CBOR txBody hash
    assert tx_hash == expected_hash, f"Transaction hash mismatch!\nExpected: {expected_hash.hex()}\nActual:   {tx_hash.hex()}"

    # Step 3: Get witness signature
    # After user approval, request witness signature
    response = client.sign_tx_witness(witness_path)
    assert response.status == Errors.SW_SUCCESS, f"Witness failed: {hex(response.status)}"

    signature = response.data
    print(f"Witness signature ({len(signature)} bytes): {signature.hex()}")

    # Step 4: Verify signature
    # The signature should be 64 bytes (ED25519)
    assert len(signature) == 64, f"Expected 64-byte signature, got {len(signature)}"

    # Verify the signature is valid
    verify_signature(witness_path, signature, tx_hash)
