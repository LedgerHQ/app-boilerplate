# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Derive Address check
"""

import pytest
import base58

from ledgered.devices import DeviceType, Device
from ragger.backend import BackendInterface
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.error import ExceptionRAPDU

from application_client.app_def import Testnet
from application_client.status_words import StatusWord
from application_client.command_sender import CommandSender
from application_client.command_builder import P1Type

from standalone.input_files.derive_address import DeriveAddressTestCase
from standalone.input_files.derive_address import byronTestCases
from standalone.input_files.derive_address import (
    shelleyTestCasesNoConfirm,
    shelleyTestCasesWithConfirm,
)
from standalone.utils import idTestFunc, derive_address

@pytest.mark.parametrize("testCase", byronTestCases, ids=idTestFunc)
def test_derive_address_byron(
    device: Device,
    backend: BackendInterface,
    navigator: Navigator,
    scenario_navigator: NavigateWithScenario,
    testCase: DeriveAddressTestCase,
) -> None:
    """Check Derive Byron Address Return"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)
    
    # TODO: check navigation instructions
    nav_inst = []
    valid_instr = []
    if device.is_nano:
        nav_inst = NavInsID.BOTH_CLICK
        valid_instr = [NavInsID.BOTH_CLICK]
    elif device.type is DeviceType.STAX:
        valid_instr += 2*[NavInsID.USE_CASE_REVIEW_TAP]
        valid_instr += [NavInsID.USE_CASE_ADDRESS_CONFIRMATION_CONFIRM]
        #valid_instr += [NavInsID.USE_CASE_REVIEW_TAP]
        
    # Send the APDU
    with client.derive_address_async(P1Type.P1_RETURN, testCase):
        if device.is_nano:
            navigator.navigate_until_text(nav_inst, valid_instr, "Confirm")
        else:
            #scenario_navigator.address_review_approve(do_comparison=False)
            navigator.navigate(valid_instr, screen_change_before_first_instruction=False)
            
    # Check the status (Asynchronous)
    response = client.get_async_response()
    

    assert response and response.status == StatusWord.SWO_SUCCESS
    encoded = base58.b58encode(response.data).decode()
    
    if testCase.netDesc == Testnet:
        assert encoded == testCase.result
    else:
        assert encoded == derive_address(testCase)

    

@pytest.mark.parametrize("testCase", byronTestCases, ids=idTestFunc)
def test_derive_address_byron_show(
    device: Device,
    backend: BackendInterface,
    navigator: Navigator,
    scenario_navigator: NavigateWithScenario,
    testCase: DeriveAddressTestCase,
) -> None:
    """Check Derive Byron Address Show"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)
    # TODO: check navigation instructions
    if device.is_nano:
        moves = []
        moves += [NavInsID.BOTH_CLICK] * 3
        moves += [NavInsID.RIGHT_CLICK]
        moves += [NavInsID.BOTH_CLICK] * 2
    elif device.type is DeviceType.STAX:
        moves = []
        moves += [NavInsID.USE_CASE_REVIEW_TAP] * 2 + [
            NavInsID.USE_CASE_ADDRESS_CONFIRMATION_TAP
        ]

    # Send the APDU
    with client.derive_address_async(P1Type.P1_DISPLAY, testCase):
        if device.is_nano:
            navigator.navigate(moves)
        else:
            #scenario_navigator.address_review_approve(do_comparison=False)
            navigator.navigate(moves, screen_change_before_first_instruction=False)
            
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == StatusWord.SWO_SUCCESS


@pytest.mark.parametrize("testCase", shelleyTestCasesNoConfirm, ids=idTestFunc)
def test_derive_address_shelley(
    backend: BackendInterface, testCase: DeriveAddressTestCase
) -> None:
    """Check Derive Shelley Address Return without confirmation"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)

    # Send the APDU
    response = client.derive_address(P1Type.P1_RETURN, testCase)
    # Check the status (Asynchronous)
    assert response and response.status == StatusWord.SWO_SUCCESS
    assert response.data == derive_address(testCase)


@pytest.mark.parametrize("testCase", shelleyTestCasesWithConfirm, ids=idTestFunc)
def test_derive_address_shelley_confirm(
    device: Device,
    backend: BackendInterface,
    navigator: Navigator,
    scenario_navigator: NavigateWithScenario,
    testCase: DeriveAddressTestCase,
) -> None:
    """Check Derive Shelley Address Return with confirmation"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)
    # TODO: check navigation instructions
    if device.is_nano:
        nav_inst = NavInsID.BOTH_CLICK
        valid_instr = [NavInsID.BOTH_CLICK]

    # Send the APDU
    with client.derive_address_async(P1Type.P1_RETURN, testCase):
        # TODO: check navigation instructions
        if device.is_nano:
            if testCase.nano_nav_confirm:
                navigator.navigate(testCase.nano_nav_confirm)
            else:
                navigator.navigate_until_text(nav_inst, valid_instr, "Confirm")
        else:
            navigator.navigate(testCase.nano_nav_confirm, screen_change_before_first_instruction=False)

    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == StatusWord.SWO_SUCCESS
    assert response.data == derive_address(testCase)

@pytest.mark.parametrize(
    "testCase",
    shelleyTestCasesNoConfirm + shelleyTestCasesWithConfirm,
    ids=idTestFunc,
)
def test_derive_address_shelley_show(
    device: Device,
    backend: BackendInterface,
    navigator: Navigator,
    scenario_navigator: NavigateWithScenario,
    testCase: DeriveAddressTestCase,
) -> None:
    """Check Derive Shelley Address Show without confirmation"""

    # TODO: check navigation instructions
    client = CommandSender(backend)

    # Send the APDU
    with client.derive_address_async(P1Type.P1_DISPLAY, testCase):

        if device.type is DeviceType.STAX:
            navigator.navigate(
                testCase.nano_nav_show, screen_change_before_first_instruction=False
            )
        else:
            if device.is_nano:
                navigator.navigate(testCase.nano_nav_show)
            else:
                scenario_navigator.address_review_approve(do_comparison=False)

    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == StatusWord.SWO_SUCCESS

@pytest.fixture(name="p1", params=[P1Type.P1_RETURN, P1Type.P1_DISPLAY])
def p1_fixture(request: pytest.FixtureRequest) -> P1Type:
    return request.param