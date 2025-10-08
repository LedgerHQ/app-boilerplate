# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for CIP36 check
"""

import pytest

from ragger.backend import BackendInterface
from ledgered.devices import Device
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

from application_client.app_def import Errors
from application_client.command_sender import CommandSender

from standalone.input_files.cvote import cvoteTestCases, CVoteTestCase

from standalone.utils import idTestFunc, verify_signature


@pytest.mark.parametrize(
    "testCase",
    cvoteTestCases,
    ids=idTestFunc
)
def test_cvote(device: Device,
               backend: BackendInterface,
               navigator: Navigator,
               scenario_navigator: NavigateWithScenario,
               testCase: CVoteTestCase) -> None:
    """Check CIP36 Vote"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)

    # Send the INIT APDU
    _cvote_init(firmware, navigator, client, testCase)

    # Send the CONFIRM APDUs
    msgData = _cvote_confirm(firmware, navigator, scenario_navigator, client)

    # Send the WITNESS APDUs
    msgSig = _cvote_witness(firmware, navigator, scenario_navigator, client, testCase)

    # Check the signatures validity
    verify_signature(testCase.cVote.witnessPath, msgSig, msgData)


def _cvote_init(device: Device,
                navigator: Navigator,
                client: CommandSender,
                testCase: CVoteTestCase) -> None:
    """cVOTE INIT

    Args:
        firmware (Firmware): The firmware version
        navigator (Navigator): The navigator instance
        client (CommandSender): The command sender instance
        testCase (CVoteTestCase): The test case
    """

    if device.is_nano:
        if device.is_nanos:
            moves = [NavInsID.RIGHT_CLICK]
            moves += [NavInsID.BOTH_CLICK] * 3
        else:
            moves = [NavInsID.BOTH_CLICK]
            moves += [NavInsID.RIGHT_CLICK]
            moves += [NavInsID.BOTH_CLICK] * 3
    else:
        moves = [NavInsID.SWIPE_CENTER_TO_LEFT]

    with client.sign_cip36_init(testCase):
        navigator.navigate(moves)
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == Errors.SW_SUCCESS

    # Send the CHUNK APDUs
    response = client.sign_cip36_chunk(testCase)
    # Check the status
    assert response and response.status == Errors.SW_SUCCESS


def _cvote_confirm(device: Device,
                   navigator: Navigator,
                   scenario_navigator: NavigateWithScenario,
                   client: CommandSender) -> bytes:
    """cVOTE CONFIRM

    Args:
        firmware (Firmware): The firmware version
        navigator (Navigator): The navigator instance
        scenario_navigator (NavigateWithScenario): the NavigateWithScenario instance
        client (CommandSender): The command sender instance

    Return:
        data hash to be signed
    """

    with client.sign_cip36_confirm():
        if device.is_nano:
            if device.is_nanos:
                moves = [NavInsID.RIGHT_CLICK]
            else:
                moves = [NavInsID.BOTH_CLICK]
            navigator.navigate(moves)
        else:
            scenario_navigator.address_review_approve(do_comparison=False)
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == Errors.SW_SUCCESS
    return response.data


def _cvote_witness(device: Device,
                   navigator: Navigator,
                   scenario_navigator: NavigateWithScenario,
                   client: CommandSender,
                   testCase: CVoteTestCase) -> bytes:
    """cVOTE WITNESS

    Args:
        firmware (Firmware): The firmware version
        navigator (Navigator): The navigator instance
        scenario_navigator (NavigateWithScenario): the NavigateWithScenario instance
        client (CommandSender): The command sender instance
        testCase (CVoteTestCase): The test case

    Return:
        data signature
    """

    with client.sign_cip36_witness(testCase):
        if device.is_nano:
            if device.is_nanos:
                moves = [NavInsID.BOTH_CLICK]
                moves += [NavInsID.RIGHT_CLICK]
            else:
                moves = [NavInsID.BOTH_CLICK] * 2
            navigator.navigate(moves)
        else:
            scenario_navigator.review_approve(do_comparison=False)
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == Errors.SW_SUCCESS
    return response.data
