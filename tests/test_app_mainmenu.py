from ragger.navigator import NavInsID, NavIns

from utils import ROOT_SCREENSHOT_PATH


# In this test we check the behavior of the device main menu
def test_app_mainmenu(firmware, navigator, test_name):
    # Navigate in the main menu
    if firmware.device.startswith("nano"):
        instructions = [
            NavIns(NavInsID.RIGHT_CLICK),
            NavIns(NavInsID.RIGHT_CLICK),
            NavIns(NavInsID.RIGHT_CLICK)
        ]
    else:
        instructions = [
            NavIns(NavInsID.USE_CASE_HOME_INFO),
            NavIns(NavInsID.USE_CASE_SETTINGS_SINGLE_PAGE_EXIT)
        ]
    navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, instructions,
                                   screen_change_before_first_instruction=False)
