from application_client.boilerplate_cmd import BoilerplateCommand
from application_client.boilerplate_response_unpacker import unpack_get_app_name_response


# In this test we check that the GET_APP_NAME replies the application name
def test_app_name(backend, firmware):
    # Use the app interface instead of raw interface
    client = BoilerplateCommand(backend)
    # Send the GET_APP_NAME instruction to the app
    response = client.get_app_name()
    # Assert that we have received the correct appname
    assert unpack_get_app_name_response(response.data) == "Boilerplate"
