from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID, NavIns


# In this test we check the behavior of the device main menu
def test_app_mainmenu(firmware: Firmware,
                      navigator: Navigator,
                      test_name: str,
                      default_screenshot_path: str) -> None:
    # Navigate in the main menu
    instructions = []
    if firmware.is_nano:
        instructions += [
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
        ]
    elif firmware is Firmware.STAX:
        instructions += [
            NavInsID.USE_CASE_HOME_SETTINGS,
            NavIns(NavInsID.TOUCH, (200, 113)),
            NavIns(NavInsID.TOUCH, (200, 261)),
            NavInsID.USE_CASE_CHOICE_CONFIRM,
            NavIns(NavInsID.TOUCH, (200, 261)),
            NavInsID.USE_CASE_SETTINGS_NEXT,
            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT
        ]
    elif firmware is Firmware.FLEX:
        instructions += [
            NavInsID.USE_CASE_HOME_SETTINGS,
            NavIns(NavInsID.TOUCH, (200, 113)),
            NavIns(NavInsID.TOUCH, (200, 300)),
            NavInsID.USE_CASE_CHOICE_CONFIRM,
            NavIns(NavInsID.TOUCH, (200, 300)),
            NavInsID.USE_CASE_SETTINGS_NEXT,
            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT
        ]

    assert len(instructions) > 0
    navigator.navigate_and_compare(default_screenshot_path, test_name, instructions,
                                   screen_change_before_first_instruction=False)
