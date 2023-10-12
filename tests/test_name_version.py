from application_client.boilerplate_command_sender import BoilerplateCommandSender
from application_client.boilerplate_response_unpacker import unpack_get_app_and_version_response


# Test a specific APDU asking BOLOS (and not the app) the name and version of the current app
def test_get_app_and_version(backend, backend_name):
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    # Send the special instruction to BOLOS
    response = client.get_app_and_version()
    # Use an helper to parse the response, assert the values
    app_name, version = unpack_get_app_and_version_response(response.data)

    assert app_name == "Boilerplate"
    assert version == "2.1.0"
