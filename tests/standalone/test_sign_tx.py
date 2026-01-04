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
    testsAlonzoTrezorComparison,
    testsBabbageTrezorComparison,
    testsAlonzo,
    testsBabbage,
    testsConwayWithoutCertificates,
    testsConwayVotingProcedures,
    testsMultidelegation,
    testsCatalystRegistration,
    testsCVoteRegistrationCIP36,
    testsMultisig,
    SignTxTestCase,
    TxAuxiliaryDataType,
    ThirdPartyAddressParams,
    TransactionSigningMode
)


def _run_sign_tx_test(device: Device,
                      backend: BackendInterface,
                      navigator: Navigator,
                      scenario_navigator: NavigateWithScenario,
                      testCase: SignTxTestCase,
                      expert_mode: bool) -> None:
    """Helper function to run a single sign_tx test iteration.

    Args:
        device: The Ledger device
        backend: The backend interface
        navigator: The navigator for UI interactions
        scenario_navigator: Scenario-based navigator
        testCase: The test case to run
        expert_mode: Whether expert mode is enabled for this run
    """
    mode_str = "expert" if expert_mode else "non-expert"
    print(f"\n{'='*60}")
    print(f"Running test in {mode_str} mode: {testCase.name}")
    print(f"{'='*60}")

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

    def _is_ordinary_witness_path(witness_path: str) -> bool:
        path_elements = witness_path.replace("'", "").split("/")
        if len(path_elements) < 2:
            return False
        try:
            purpose = int(path_elements[1])
        except ValueError:
            return False
        return purpose in (44, 1852)

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

        # Expert mode shows ordinary witness paths that are hidden otherwise.
        should_confirm_witness = len(moves) > 0 or (expert_mode and _is_ordinary_witness_path(path))
        if should_confirm_witness and device.is_nano and len(moves) == 0:
            moves = [NavInsID.BOTH_CLICK]

        # Each witness requires explicit confirmation on the device
        with client.sign_tx_witness_async(path):
            if should_confirm_witness:
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


@pytest.mark.parametrize(
    "expert_mode",
    [False, True],
    ids=["non_expert", "expert"]
)
@pytest.mark.parametrize(
    "testCase",
    testsByron + testsMary + testsShelleyNoCertificates + testsShelleyWithCertificates +
    testsAllegra + testsAlonzoTrezorComparison + testsBabbageTrezorComparison +
    testsAlonzo + testsBabbage + testsConwayWithCertificates +
    testsConwayWithoutCertificates + testsConwayVotingProcedures +
    testsMultidelegation + testsCatalystRegistration + testsCVoteRegistrationCIP36 +
    testsMultisig,
    ids=idTestFunc
)
def test_sign_tx(device: Device,
                 backend: BackendInterface,
                 navigator: Navigator,
                 scenario_navigator: NavigateWithScenario,
                 testCase: SignTxTestCase,
                 expert_mode: bool) -> None:
    """Test transaction signing under a specific expert mode setting.

    NOTE: This test assumes a DEBUG build because it uses the debug settings APDU.
    For production builds, drop the debug APDU call and run only the standard UI flow.

    Each run performs:
    1. Set expert mode via debug APDU (only works with DEBUG builds)
    2. Send init APDU with transaction description
    3. Send transaction data in unpacked format
    4. User approves transaction
    5. Request witness signature
    """

    client = CommandSender(backend)
    client.set_debug_settings(expert_mode=expert_mode, silent_export=False)

    try:
        _run_sign_tx_test(device, backend, navigator, scenario_navigator, testCase, expert_mode=expert_mode)
    except Exception as e:
        mode_label = "EXPERT MODE" if expert_mode else "NON-EXPERT MODE"
        raise AssertionError(f"Test FAILED in {mode_label}: {testCase.name}") from e
