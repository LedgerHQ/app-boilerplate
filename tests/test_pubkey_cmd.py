from application_client.boilerplate_cmd import BoilerplateCommand, Errors
from application_client.boilerplate_response_unpacker import unpack_get_public_key_response
from ragger.bip import calculate_public_key_and_chaincode, CurveChoice
from ragger.backend import RaisePolicy
from utils import create_simple_nav_instructions, ROOT_SCREENSHOT_PATH


# In this test we check that the GET_PUBLIC_KEY works in non-confirmation mode
def test_get_public_key_no_confirm(backend, firmware):
    for path in ["m/44'/0'/0'/0/0", "m/44'/0'/0/0/0", "m/44'/0'/911'/0/0", "m/44'/0'/255/255/255", "m/44'/0'/2147483647/0/0/0/0/0/0/0"]:
        client = BoilerplateCommand(backend)
        response = client.get_public_key(path=path).data
        _, public_key, _, chain_code = unpack_get_public_key_response(response)

        ref_public_key, ref_chain_code = calculate_public_key_and_chaincode(CurveChoice.Secp256k1, path=path)
        assert public_key.hex() == ref_public_key
        assert chain_code.hex() == ref_chain_code


# In this test we check that the GET_PUBLIC_KEY works in confirmation mode
def test_get_public_key_confirm_accepted(backend, firmware, navigator, test_name):
    client = BoilerplateCommand(backend)
    path="m/44'/0'/0'/0/0"
    with client.get_public_key_with_confirmation(path=path):
        if backend.firmware.device == "nanos":
            nav_ins = create_simple_nav_instructions(5)
        elif backend.firmware.device.startswith("nano"):
            nav_ins = create_simple_nav_instructions(3)
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins)
    response = client.get_async_response().data
    _, public_key, _, chain_code = unpack_get_public_key_response(response)

    ref_public_key, ref_chain_code = calculate_public_key_and_chaincode(CurveChoice.Secp256k1, path=path)
    assert public_key.hex() == ref_public_key
    assert chain_code.hex() == ref_chain_code


# In this test we check that the GET_PUBLIC_KEY in confirmation mode replies an error if the user refuses
def test_get_public_key_confirm_refused(backend, firmware, navigator, test_name):
    client = BoilerplateCommand(backend)
    path="m/44'/0'/0'/0/0"
    with client.get_public_key_with_confirmation(path=path):
        if backend.firmware.device == "nanos":
            nav_ins = create_simple_nav_instructions(5 + 1)
        elif backend.firmware.device.startswith("nano"):
            nav_ins = create_simple_nav_instructions(3 + 1)
        # Disable raising when trying to unpack an error APDU
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins)

    response = client.get_async_response()

    # Assert that we have received a refusal
    assert response.status == Errors.SW_DENY
    assert len(response.data) == 0
