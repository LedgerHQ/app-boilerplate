from ragger.navigator import NavInsID, NavIns

from utils import ROOT_SCREENSHOT_PATH


# In this test we check the behavior of the device main menu
def test_app_mainmenu(firmware, navigator, test_name):
    # Navigate in the main menu
    if firmware.device.startswith("nano"):
        instructions = [
            NavInsID.RIGHT_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.RIGHT_CLICK
        ]
    else:
        instructions = [
            NavInsID.USE_CASE_HOME_SETTINGS,
            NavInsID.USE_CASE_SETTINGS_NEXT,
            NavIns(NavInsID.TOUCH, (200, 113)),
            NavIns(NavInsID.TOUCH, (200, 261)),
            NavInsID.USE_CASE_CHOICE_CONFIRM,
            NavInsID.USE_CASE_SETTINGS_NEXT,
            NavIns(NavInsID.TOUCH, (200, 261)),
            NavIns(NavInsID.TOUCH, (200, 261)),
            NavInsID.USE_CASE_CHOICE_REJECT,
            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT
        ]
    navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, instructions,
                                   screen_change_before_first_instruction=False)
