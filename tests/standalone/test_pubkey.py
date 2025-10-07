import pytest

from ledgered.devices import Device
from ragger.backend import BackendInterface
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.error import ExceptionRAPDU

from application_client.command_sender import CommandSender, Errors

from standalone.input_files.pubkey import PubKeyTestCase, rejectTestCases, testsByron, testsShelleyUsual, testsShelleyUnusual, testsColdKeys, testsCVoteKeysUsual, testsCVoteKeysUnusual

from standalone.utils import get_navigation_for_toggle_silent_pubkey_export, idTestFunc, get_device_pubkey

@pytest.mark.parametrize(
    "testCase",
    testsByron + testsShelleyUsual + testsShelleyUnusual + testsColdKeys + testsCVoteKeysUsual + testsCVoteKeysUnusual,
    ids=idTestFunc
)
def test_pubkey_confirm(device: Device,
                        backend: BackendInterface,
                        navigator: Navigator,
                        scenario_navigator: NavigateWithScenario,
                        testCase: PubKeyTestCase) -> None:
    """Check Public Key with confirmation"""

    # turn off silent pubkey export, confirmation will be asked for each key
    nav_instructions = get_navigation_for_toggle_silent_pubkey_export(device)
    navigator.navigate(nav_instructions, screen_change_before_first_instruction=False)

    # Use the app interface instead of raw interface
    client = CommandSender(backend)
    with client.get_pubkey_async(testCase.path):
        if testCase.nav:
            if device.is_nano:
                # For Nano devices: navigate to "Export" and click it
                navigator.navigate_until_text(NavInsID.RIGHT_CLICK, [NavInsID.BOTH_CLICK], "Export")
            else:
                # For NanoS+/Stax: TODO - determine correct navigation pattern
                # Likely uses USE_CASE_CHOICE_CONFIRM or similar
                navigator.navigate_until_text(NavInsID.RIGHT_CLICK, [NavInsID.USE_CASE_CHOICE_CONFIRM], "Export")
        else:
            pass
    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == Errors.SW_SUCCESS

    # Check the response
    _check_pubkey_result(response.data, testCase.path)


@pytest.mark.parametrize(
    "testCase",
    testsShelleyUsual + testsCVoteKeysUsual,
    ids=idTestFunc
)
def test_pubkey_without_confirmation(backend: BackendInterface, testCase: PubKeyTestCase) -> None:
    """Check Public Key without confirmation"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)

    with client.get_pubkey_async(testCase.path):
        pass

    # Check the status (Asynchronous)
    response = client.get_async_response()
    assert response and response.status == Errors.SW_SUCCESS

    # Check the response
    _check_pubkey_result(response.data, testCase.path)


@pytest.mark.parametrize(
    "testCase",
    rejectTestCases,
    ids=idTestFunc
)
def test_pubkey_reject(backend: BackendInterface,
                       testCase: PubKeyTestCase) -> None:
    """Check Reject Public Key"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)

    with pytest.raises(ExceptionRAPDU) as err:
        with client.get_pubkey_async(testCase.path):
            pass
    assert err.value.status == Errors.SW_REJECTED_BY_POLICY


def _check_pubkey_result(data: bytes, path: str) -> None:
    ref_pk, ref_chaincode = get_device_pubkey(path)
    assert data.hex() == ref_pk.hex() + ref_chaincode
