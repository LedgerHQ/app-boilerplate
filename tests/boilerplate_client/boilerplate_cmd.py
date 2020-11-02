import struct
from typing import Tuple

from ledgercomm import Transport

from boilerplate_client.boilerplate_cmd_builder import BoilerplateCommandBuilder, InsType
from boilerplate_client.exception import DeviceException


class BoilerplateCommand:
    def __init__(self,
                 transport: Transport,
                 debug: bool = False) -> None:
        self.transport = transport
        self.builder = BoilerplateCommandBuilder(debug=debug)
        self.debug = debug

    def get_version(self) -> Tuple[int, int, int]:
        sw, response = self.transport.exchange_raw(
            self.builder.get_version()
        )  # type: int, bytes

        if not sw & 0x9000:
            raise DeviceException(error_code=sw, ins=InsType.INS_GET_VERSION)

        assert len(response) == 3

        major, minor, patch = struct.unpack(
            "BBB",
            response
        )  # type: int, int, int

        return major, minor, patch

    def get_app_name(self) -> str:
        sw, response = self.transport.exchange_raw(
            self.builder.get_app_name()
        )  # type: int, bytes

        if not sw & 0x9000:
            raise DeviceException(error_code=sw, ins=InsType.INS_GET_APP_NAME)

        return response.decode("ascii")
