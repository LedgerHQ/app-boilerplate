# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Operational Certificate check
"""

import pytest

from ledgered.devices import Device
from ragger.backend import BackendInterface
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

from application_client.app_def import Errors
from application_client.command_sender import CommandSender

from standalone.input_files.signOpCert import opCertTestCases, OpCertTestCase

from standalone.utils import idTestFunc, verify_signature


@pytest.mark.parametrize(
    "testCase",
    opCertTestCases,
    ids=idTestFunc
)
def test_opCert(device: Device,
                backend: BackendInterface,
                navigator: Navigator,
                scenario_navigator: NavigateWithScenario,
                testCase: OpCertTestCase) -> None:
    """Check Sign Operational Certificate"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)

    # Send the INIT APDU
    with client.sign_opCert(testCase):
        if device.is_nano:
            # TODO warning not shown ???
            navigator.navigate_until_text(NavInsID.RIGHT_CLICK, [NavInsID.BOTH_CLICK], "Sign certificate")
        else:
            if testCase.warning:
                scenario_navigator.review_approve_with_warning(do_comparison=False)
            else:
                scenario_navigator.review_approve(do_comparison=False)
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == Errors.SW_SUCCESS

    # Check the response
    assert response.data.hex() == testCase.expected.signatureHex

    msg = bytes()
    msg += bytes.fromhex(testCase.opCert.kesPublicKeyHex)
    msg += testCase.opCert.issueCounter.to_bytes(8, 'big')
    msg += testCase.opCert.kesPeriod.to_bytes(8, 'big')

    verify_signature(testCase.opCert.path, response.data, msg)
