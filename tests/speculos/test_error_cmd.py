import pytest

from speculos.client import ApduException

from boilerplate_client.exception import *


@pytest.mark.xfail(raises=ClaNotSupportedError)
def test_bad_cla(client):
    try:
        client.apdu_exchange(cla=0xa0,  # 0xa0 instead of 0xe0
                             ins=0x03)
    except ApduException as error:
        raise DeviceException(error_code=error.sw)


@pytest.mark.xfail(raises=InsNotSupportedError)
def test_bad_ins(client):
    try:
        client.apdu_exchange(cla=0xe0,
                             ins=0xff)  # bad INS
    except ApduException as error:
        raise DeviceException(error_code=error.sw)


@pytest.mark.xfail(raises=WrongP1P2Error)
def test_wrong_p1p2(client):
    try:
        client.apdu_exchange(cla=0xe0,
                             ins=0x03,
                             p1=0x01)  # 0x01 instead of 0x00
    except ApduException as error:
        raise DeviceException(error_code=error.sw)


@pytest.mark.xfail(raises=WrongDataLengthError)
def test_wrong_data_length(client):
    try:
        # APDUs must be at least 5 bytes: CLA, INS, P1, P2, Lc.
        client._apdu_exchange(b"E000")
    except ApduException as error:
        raise DeviceException(error_code=error.sw)
