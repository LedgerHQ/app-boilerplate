from dataclasses import dataclass
from functools import partial

from ragger.error import ExceptionRAPDU


@dataclass(frozen=True)
class BoilerplateError(ExceptionRAPDU):
    error_message: str = str()

class UnknownDeviceError(Exception):
    pass

DenyError = partial(BoilerplateError, 0x6985, 'SW_DENY')
WrongP1P2Error = partial(BoilerplateError, 0x6A86, 'SW_WRONG_P1P2')
WrongDataLengthError = partial(BoilerplateError, 0x6A87, 'SW_WRONG_DATA_LENGTH')
InsNotSupportedError = partial(BoilerplateError, 0x6D00, 'SW_INS_NOT_SUPPORTED')
ClaNotSupportedError = partial(BoilerplateError, 0x6E00, 'SW_CLA_NOT_SUPPORTED')
WrongResponseLengthError = partial(BoilerplateError, 0xB000, 'SW_WRONG_RESPONSE_LENGTH')
DisplayBip32PathFailError = partial(BoilerplateError, 0xB001, 'SW_DISPLAY_BIP32_PATH_FAIL')
DisplayAddressFailError = partial(BoilerplateError, 0xB002, 'SW_DISPLAY_ADDRESS_FAIL')
DisplayAmountFailError = partial(BoilerplateError, 0xB003, 'SW_DISPLAY_AMOUNT_FAIL')
WrongTxLengthError = partial(BoilerplateError, 0xB004, 'SW_WRONG_TX_LENGTH')
TxParsingFailError = partial(BoilerplateError, 0xB005, 'SW_TX_PARSING_FAIL')
TxHashFail = partial(BoilerplateError, 0xB006, 'SW_TX_HASH_FAIL')
BadStateError = partial(BoilerplateError, 0xB007, 'SW_BAD_STATE')
SignatureFailError = partial(BoilerplateError, 0xB008, 'SW_SIGNATURE_FAIL')


ERRORS = [
    DenyError,
    WrongP1P2Error,
    WrongDataLengthError,
    InsNotSupportedError,
    ClaNotSupportedError,
    WrongResponseLengthError,
    DisplayBip32PathFailError,
    DisplayAddressFailError,
    DisplayAmountFailError,
    WrongTxLengthError,
    TxParsingFailError,
    TxHashFail,
    BadStateError,
    SignatureFailError
]
