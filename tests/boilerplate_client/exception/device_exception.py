import enum
from typing import Union

from .errors import ERRORS, UnknownDeviceError


class DeviceException(Exception):  # pylint: disable=too-few-public-methods
    def __new__(
            cls,
            error_code: int,
            ins: Union[int, enum.IntEnum, None] = None,
            message: str = ""
    ) -> "DeviceException":
        error_message: str = (f"Error in {ins!r} command"
                              if ins else "Error in command")
        for partial_error in ERRORS:
            if partial_error.args[0] == error_code:
                return partial_error(message, error_message)
        return UnknownDeviceError(hex(error_code), error_message, message)
