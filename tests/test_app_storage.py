import pytest
from ragger.backend import SpeculosBackend
from ragger.firmware import Firmware
from ragger.navigator import NavInsID, NavIns
from ragger.navigator import NanoNavigator, TouchNavigator

def get_navigator(
        backend: SpeculosBackend,
        golden_run: bool):
    NavigatorType = TouchNavigator if backend.firmware == Firmware.STAX else NanoNavigator
    return NavigatorType(backend, backend.firmware, golden_run)

# In this test we check app persistent storage functionality supported by Speculos
def test_app_storage_verification(
        firmware: Firmware,
        get_2_backends,
        backend_name: str,
        golden_run: bool,
        test_name: str,
        default_screenshot_path: str) -> None:

    if backend_name != "speculos":
        pytest.skip("Requires Speculos")

    b_setup, b_with_load_nvram = get_2_backends

    # Navigating in the main menu and creating some persistent data
    with b_setup:
        navigator1 = get_navigator(b_setup, golden_run)
        instructions = []
        if firmware is Firmware.STAX:
            instructions += [
                NavInsID.USE_CASE_HOME_SETTINGS,
                NavIns(NavInsID.TOUCH, (200, 113)),
                NavIns(NavInsID.TOUCH, (200, 261)),
                NavInsID.USE_CASE_CHOICE_CONFIRM,
                NavIns(NavInsID.TOUCH, (200, 261)),
                NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT
            ]
        assert len(instructions) > 0
        navigator1.navigate_and_compare(default_screenshot_path, test_name + "_setup", instructions,
                                       screen_change_before_first_instruction=False)

    # Pre-loading the persistent data and checking it
    with b_with_load_nvram:
        navigator2 = get_navigator(b_with_load_nvram, golden_run)
        instructions = []
        if firmware is Firmware.STAX:
            instructions += [
                NavInsID.USE_CASE_HOME_SETTINGS,
                NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT
            ]
        assert len(instructions) > 0
        navigator2.navigate_and_compare(default_screenshot_path, test_name + "_check", instructions,
                                       screen_change_before_first_instruction=False)
