from ragger.backend.interface import BackendInterface

from application_client.boilerplate_command_sender import BoilerplateCommandSender
from application_client.boilerplate_response_unpacker import unpack_get_version_response

from utils import verify_version

# In this test we check the behavior of the device when asked to provide the app version
def test_version(backend: BackendInterface) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    # Send the GET_VERSION instruction
    rapdu = client.get_version()
    # Use an helper to parse the response, assert the values
    MAJOR, MINOR, PATCH = unpack_get_version_response(rapdu.data)
    verify_version(f"{MAJOR}.{MINOR}.{PATCH}")
