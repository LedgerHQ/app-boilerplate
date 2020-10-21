import enum
import struct

from ledgercomm import Transport

from exception import DeviceException


CLA: int = 0xe0


class InsType(enum.IntEnum):
    INS_GET_VERSION = 0x03
    INS_GET_APP_NAME = 0x04


class Command:
    def __init__(self,
                 transport: Transport,
                 debug: bool = False) -> None:
        self.transport = transport
        self.debug = debug
    
    def get_version(self) -> str:
        ins: InsType = InsType.INS_GET_VERSION

        self.transport.send(cla=CLA,
                            ins=ins,
                            p1=0x00,
                            p2=0x00,
                            payload=b"")
        
        sw, response = self.transport.recv()  # type: int, bytes

        if not sw & 0x9000:
            raise DeviceException(error_code=sw, ins=ins)

        assert len(response) == 3

        major, minor, patch = struct.unpack(
            "BBB",
            response
        )  # type: int, int, int

        return f"{major}.{minor}.{patch}"

    def get_app_name(self) -> str:
        ins: InsType = InsType.INS_GET_APP_NAME

        self.transport.send(cla=CLA,
                            ins=ins,
                            p1=0x00,
                            p2=0x00,
                            payload=b"")
        
        sw, response = self.transport.recv()  # type: int, bytes

        if not sw & 0x9000:
            raise DeviceException(error_code=sw, ins=ins)

        return response.decode("ascii")
