from ragger.navigator import NavInsID, NavIns

from utils import ROOT_SCREENSHOT_PATH


# In this test we check the behavior of the device main menu
def test_app_mainmenu(navigator, test_name):
    # Navigate in the main menu
    instructions = [
        NavIns(NavInsID.RIGHT_CLICK),
        NavIns(NavInsID.RIGHT_CLICK),
        NavIns(NavInsID.RIGHT_CLICK)
    ]
    navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, instructions,
                                   screen_change_before_first_instruction=False)
