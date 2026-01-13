# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Derive Native Script Hash check
"""

import pytest

from ragger.backend import BackendInterface
from ledgered.devices import DeviceType, Device
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.error import ExceptionRAPDU

from application_client.status_words import StatusWord
from application_client.command_sender import CommandSender

from standalone.input_files.derive_native_script import ValidNativeScriptTestCases, ValidNativeScriptTestCase
from standalone.input_files.derive_native_script import NativeScript, NativeScriptType
from standalone.input_files.derive_native_script import NativeScriptParamsPubkey, NativeScriptHashDisplayFormat
from standalone.input_files.derive_native_script import NativeScriptParamsScripts, NativeScriptParamsNofK

from standalone.utils import idTestFunc


@pytest.mark.parametrize(
    "testCase",
    ValidNativeScriptTestCases,
    ids=idTestFunc
)
def test_derive_native_script_hash(device: Device,
                                   backend: BackendInterface,
                                   navigator: Navigator,
                                   scenario_navigator: NavigateWithScenario,
                                   testCase: ValidNativeScriptTestCase) -> None:
    """Check Derive Native Script Hash"""

    if device.is_nano and testCase.nano_skip is True:
        pytest.skip("Not supported yet on Nano because Navigation should be reviewed")

    # Use the app interface instead of raw interface
    client = CommandSender(backend)

    _deriveNativeScriptHash_addScript(device, navigator, client, testCase.script, False)

    _deriveNativeScriptHash_finishWholeNativeScript(device, navigator, scenario_navigator, client, testCase)


def _deriveNativeScriptHash_addScript(device: Device,
                                      navigator: Navigator,
                                      client: CommandSender,
                                      script: NativeScript,
                                      complex_nav: bool) -> None:
    """Send the different add commands

    Args:
        firmware (Firmware): The firmware version
        navigator (Navigator): The navigator instance
        client (CommandSender): The command sender instance
        script (NativeScript): The test case
        complex_nav (bool): The complex navigation flag
    """

    if script.type in [NativeScriptType.ALL, NativeScriptType.ANY, NativeScriptType.N_OF_K]:
        _deriveScriptHash_startComplexScript(device, navigator, client, script, complex_nav)
        assert isinstance(script.params, (NativeScriptParamsScripts, NativeScriptParamsNofK))
        for subscript in script.params.scripts:
            _deriveNativeScriptHash_addScript(device, navigator, client, subscript, True)
    else:
        _deriveNativeScriptHash_addSimpleScript(device, navigator, client, script, complex_nav)


def _deriveNativeScriptHash_addSimpleScript(device: Device,
                                            navigator: Navigator,
                                            client: CommandSender,
                                            script: NativeScript,
                                            complex_nav: bool) -> None:
    """Send the add command for a simple script

    Args:
        firmware (Firmware): The firmware version
        navigator (Navigator): The navigator instance
        client (CommandSender): The command sender instance
        script (NativeScript): The script
        complex_nav (bool): The complex navigation flag
    """

    with client.derive_script_add_simple(script):
        """
            moves = []
            if device.is_nano:
                if complex_nav:
                    moves += [NavInsID.BOTH_CLICK]
                if complex_nav or script.type == NativeScriptType.PUBKEY_THIRD_PARTY:
                    moves += [NavInsID.RIGHT_CLICK]
                moves += [NavInsID.BOTH_CLICK]
                #navigator.navigate(moves)
            else:
                if complex_nav:
                    moves += [NavInsID.TAPPABLE_CENTER_TAP]
                moves += [NavInsID.SWIPE_CENTER_TO_LEFT]
                #navigator.navigate(moves,
                #                   screen_change_before_first_instruction=False,
                #                   screen_change_after_last_instruction=False)
        """
        
        moves = []
        moves += [NavInsID.USE_CASE_REVIEW_TAP]
        if device.type is DeviceType.STAX and navigator is not None:
            navigator.navigate(
                moves, screen_change_before_first_instruction=False
            )
        
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == StatusWord.SWO_SUCCESS


def _deriveScriptHash_startComplexScript(device: Device,
                                         navigator: Navigator,
                                         client: CommandSender,
                                         script: NativeScript,
                                         complex_nav: bool) -> None:
    """Send the add command for a complex script

    Args:
        firmware (Firmware): The firmware version
        client (CommandSender): The command sender instance
        navigator (Navigator): The navigator instance
        script (NativeScript): The script
        complex_nav (bool): The complex navigation flag
    """

    with client.derive_script_add_complex(script):
        """
        moves = []
        if device.is_nano:
            if complex_nav:
                moves += [NavInsID.BOTH_CLICK]
            if complex_nav or isinstance(script.params, NativeScriptParamsPubkey):
                moves += [NavInsID.RIGHT_CLICK]
            moves += [NavInsID.BOTH_CLICK]
        else:
            if complex_nav:
                moves += [NavInsID.TAPPABLE_CENTER_TAP]
            moves += [NavInsID.SWIPE_CENTER_TO_LEFT]
        navigator.navigate(moves)
        """
    
        moves = []
        moves += [NavInsID.USE_CASE_REVIEW_TAP]
        if device.type is DeviceType.STAX and navigator is not None:
            navigator.navigate(
                moves, screen_change_before_first_instruction=False
            )
        
    # Check the status (Asynchronous)
    
    response = client.get_async_response()
    assert response and response.status == StatusWord.SWO_SUCCESS


def _deriveNativeScriptHash_finishWholeNativeScript(device: Device,
                                                    navigator: Navigator,
                                                    scenario_navigator: NavigateWithScenario,
                                                    client: CommandSender,
                                                    testCase: ValidNativeScriptTestCase) -> None:
    """Send the finish command for the whole native script

    Args:
        firmware (Firmware): The firmware version
        navigator (Navigator): The navigator instance
        scenario_navigator (NavigateWithScenario): The scenario navigator instance
        client (CommandSender): The command sender instance
        testCase (ValidNativeScriptTestCase): The test case
    """

    with client.derive_script_finish(testCase.displayFormat):
        """
        if device.is_nano:
            moves = []
            if testCase.script.type in (NativeScriptType.INVALID_BEFORE, NativeScriptType.INVALID_HEREAFTER):
                if testCase.script.params.slot > 1000:
                    moves += [NavInsID.RIGHT_CLICK]
            elif testCase.displayFormat != NativeScriptHashDisplayFormat.POLICY_ID and \
                not (testCase.script.type == NativeScriptType.N_OF_K and testCase.script.params.requiredCount > 0):
                moves += [NavInsID.RIGHT_CLICK]
            moves += [NavInsID.BOTH_CLICK]

            navigator.navigate(moves)
        else:
            scenario_navigator.address_review_approve(do_comparison=False)
        """
        """
        moves = []
        if device.type is DeviceType.STAX and navigator is not None:
            navigator.navigate(
                moves, screen_change_before_first_instruction=False
            )
        """
        moves = []
        moves += [NavInsID.USE_CASE_REVIEW_TAP]
        moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        if device.type is DeviceType.STAX and navigator is not None:
            navigator.navigate(
                moves, screen_change_before_first_instruction=False
            )
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == StatusWord.SWO_SUCCESS
    # Check the response
    assert response.data.hex() == testCase.expected.hash
    # TODO: Generate the payload and verify the signature
