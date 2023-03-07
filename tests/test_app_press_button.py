from ragger.navigator import NavInsID

from utils import ROOT_SCREENSHOT_PATH


# In this test we check the behavior of the device main menu
def test_app_press_button(firmware, navigator, test_name):
    # Navigate in the main menu
    if firmware.device.startswith("nano"):
        return
    else:
        instructions = [
            NavInsID.USE_CASE_CHOICE_CONFIRM,
            NavInsID.USE_CASE_CHOICE_CONFIRM,
            NavInsID.USE_CASE_CHOICE_CONFIRM
        ]
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, instructions,
                                    screen_change_before_first_instruction=False)


# In this test we check the behavior of the device main menu
def test_app_press_button2(firmware, navigator, test_name):
    # Navigate in the main menu
    if firmware.device.startswith("nano"):
        return
    else:
        instructions = [
            NavInsID.USE_CASE_CHOICE_CONFIRM,
            NavInsID.USE_CASE_CHOICE_REJECT
        ]
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, instructions,
                                    screen_change_before_first_instruction=False)



