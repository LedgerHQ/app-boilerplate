import enum
import logging
import struct
from typing import Union, cast


class InsType(enum.IntEnum):
    INS_GET_VERSION = 0x03
    INS_GET_APP_NAME = 0x04


class BoilerplateCommandBuilder:
    """APDU command builder for the Boilerplate application.

    Parameters
    ----------
    debug: bool
        Whether you want to see logging or not.

    Attributes
    ----------
    debug: bool
        Whether you want to see logging or not.

    """
    CLA: int = 0xE0

    def __init__(self, debug: bool = False):
        """Init constructor."""
        self.debug = debug

    def serialize(self,
                  cla: int,
                  ins: Union[int, enum.IntEnum],
                  p1: int = 0,
                  p2: int = 0,
                  cdata: bytes = b"") -> bytes:
        """Serialize the whole APDU command (header + data).

        Parameters
        ----------
        cla : int
            Instruction class: CLA (1 byte)
        ins : Union[int, IntEnum]
            Instruction code: INS (1 byte)
        p1 : int
            Instruction parameter 1: P1 (1 byte).
        p2 : int
            Instruction parameter 2: P2 (1 byte).
        cdata : bytes
            Bytes of command data.

        Returns
        -------
        bytes
            Bytes of a complete APDU command.

        """
        ins = cast(int, ins.value) if isinstance(ins, enum.IntEnum) else cast(int, ins)

        header: bytes = struct.pack("BBBBB",
                                    cla,
                                    ins,
                                    p1,
                                    p2,
                                    len(cdata))  # add Lc to APDU header

        if self.debug:
            logging.info("header: %s", header.hex())
            logging.info("cdata:  %s", cdata.hex())

        return header + cdata

    def get_version(self) -> bytes:
        """Command builder for GET_VERSION.

        Returns
        -------
        bytes
            APDU command for GET_VERSION.

        """
        return self.serialize(cla=self.CLA,
                              ins=InsType.INS_GET_VERSION,
                              p1=0x00,
                              p2=0x00,
                              cdata=b"")

    def get_app_name(self) -> bytes:
        """Command builder for GET_APP_NAME.

        Returns
        -------
        bytes
            APDU command for GET_APP_NAME.

        """
        return self.serialize(cla=self.CLA,
                              ins=InsType.INS_GET_APP_NAME,
                              p1=0x00,
                              p2=0x00,
                              cdata=b"")
