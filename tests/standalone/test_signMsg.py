# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Sign Message check
"""

from hashlib import blake2b
import pytest
import cbor

from ragger.backend import BackendInterface
from ledgered.devices import Device
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

from application_client.app_def import Errors, AddressType, Mainnet
from application_client.command_sender import CommandSender

from standalone.input_files.signMsg import signMsgTestCases, SignMsgTestCase, MessageAddressFieldType

from standalone.test_derive_address import DeriveAddressTestCase

from standalone.utils import pop_sized_buf_from_buffer, pop_size_prefixed_buf_from_buf
from standalone.utils import idTestFunc, get_device_pubkey, verify_signature, derive_address


@pytest.mark.parametrize(
    "testCase",
    signMsgTestCases,
    ids=idTestFunc
)
def test_sign_message(device: Device,
                      backend: BackendInterface,
                      navigator: Navigator,
                      scenario_navigator: NavigateWithScenario,
                      testCase: SignMsgTestCase) -> None:
    """Check Sign Message"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)

    # Send the INIT APDU
    _signMsg_init(firmware, navigator, client, testCase)

    # Send the CHUNK APDUs
    _signMsg_chunk(firmware, backend, navigator, client, testCase)

    # Send the CONFIRM APDUs
    signedData = _signMsg_confirm(firmware, navigator, scenario_navigator, client, testCase)

    # Check the response
    _check_result(testCase, signedData)


def _signMsg_init(device: Device,
                  navigator: Navigator,
                  client: CommandSender,
                  testCase: SignMsgTestCase) -> None:
    """Sign Message INIT

    Args:
        firmware (Firmware): The firmware version
        navigator (Navigator): The navigator instance
        client (CommandSender): The command sender instance
        testCase (SignMsgTestCase): The test case
    """

    with client.sign_msg_init(testCase):
        if device.is_nano:
            if device.is_nanos:
                moves = [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK] * 2
            else:
                moves = testCase.nav.init
            navigator.navigate(moves)
        else:
            navigator.navigate([NavInsID.SWIPE_CENTER_TO_LEFT])
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == Errors.SW_SUCCESS


def _signMsg_chunk(device: Device,
                   backend: BackendInterface,
                   navigator: Navigator,
                   client: CommandSender,
                   testCase: SignMsgTestCase) -> None:
    """Sign Message CHUNK

    Args:
        firmware (Firmware): The firmware version
        navigator (Navigator): The navigator instance
        client (CommandSender): The command sender instance
        testCase (SignMsgTestCase): The test case
    """

    with client.sign_msg_chunk(testCase):
        if device.is_nano:
            if device.is_nanos:
                moves = [NavInsID.BOTH_CLICK]
                if testCase.msgData.messageHex:
                    moves += [NavInsID.BOTH_CLICK]
            else:
                moves = testCase.nav.chunk
            navigator.navigate(moves)
        else:
            if len(testCase.msgData.messageHex) > 0:
                backend.wait_for_text_not_on_screen("Processing")
            navigator.navigate([NavInsID.TAPPABLE_CENTER_TAP],
                               screen_change_before_first_instruction=False,
                               screen_change_after_last_instruction=False)
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == Errors.SW_SUCCESS


def _signMsg_confirm(device: Device,
                     navigator: Navigator,
                     scenario_navigator: NavigateWithScenario,
                     client: CommandSender,
                     testCase: SignMsgTestCase) -> bytes:
    """Sign Message CONFIRM

    Args:
        firmware (Firmware): The firmware version
        navigator (Navigator): The navigator instance
        client (CommandSender): The command sender instance
        testCase (SignMsgTestCase): The test case

    Return:
        Signed data
    """

    with client.sign_msg_confirm():
        if device.is_nano:
            if device.is_nanos:
                moves = [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK]
            else:
                moves = testCase.nav.confirm
            navigator.navigate(moves)
        else:
            scenario_navigator.address_review_approve(do_comparison=False)
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == Errors.SW_SUCCESS
    return response.data


def _check_result(testCase: SignMsgTestCase, buffer: bytes) -> None:
    """Check the response, containing
    - ED25519 signature (64 bytes)
    - Public key (32 bytes)
    - Address field size (4 bytes)
    - Address field (Up to 128 bytes)
    """

    ED25519_SIGNATURE_LENGTH = 64
    PUBLIC_KEY_LENGTH = 32
    MAX_ADDRESS_SIZE = 128
    # Check the response length
    assert len(buffer) <= ED25519_SIGNATURE_LENGTH + PUBLIC_KEY_LENGTH + 4 + MAX_ADDRESS_SIZE
    # Get the signature
    buffer, signature = pop_sized_buf_from_buffer(buffer, ED25519_SIGNATURE_LENGTH)
    # Get the public key
    buffer, signingPublicKey = pop_sized_buf_from_buffer(buffer, PUBLIC_KEY_LENGTH)
    # Get the address field
    buffer, _, addressField = pop_size_prefixed_buf_from_buf(buffer, 4)

    # Check the public key
    pk, _ = get_device_pubkey(testCase.msgData.signingPath)
    assert signingPublicKey == pk

    # Check the address field
    if testCase.msgData.addressFieldType == MessageAddressFieldType.ADDRESS:
        assert addressField == derive_address(testCase.msgData.addressDesc)
    else:
        address = derive_address(DeriveAddressTestCase("",
                                                       Mainnet,
                                                       AddressType.BASE_PAYMENT_KEY_STAKE_KEY,
                                                       testCase.msgData.signingPath))
        assert addressField == address[1:]

    # Check the signature
    payload = _generate_payload(testCase, addressField)
    verify_signature(testCase.msgData.signingPath, signature, payload)


def _generate_payload(testCase: SignMsgTestCase, addressField: bytes) -> bytes:
    """Generate the payload to sign

    Args:
        testCase (SignMsgTestCase): The test case

    Return:
        The payload
    """

    array = []
    dico = {
        1: -8,
        "address": addressField
    }

    array.append("Signature1")
    array.append(cbor.cbor.dumps_dict(dico))
    array.append(b'')
    if testCase.msgData.hashPayload:
        msgHash = blake2b(bytes.fromhex(testCase.msgData.messageHex), digest_size=28).hexdigest()
        array.append(bytes.fromhex(msgHash))
    else:
        array.append(bytes.fromhex(testCase.msgData.messageHex))

    return cbor.cbor.dumps_array(array)
