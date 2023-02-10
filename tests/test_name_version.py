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

    # for now, Speculos always returns app:1.33.7, so this test is not very meaningful
    # however, its failure will mean that Speculos may have been improved on this front
    if backend_name == "speculos":
        assert app_name == "app"
        assert version == "1.33.7"
    else:
        assert app_name == "Boilerplate"
        assert version == "1.0.1"
