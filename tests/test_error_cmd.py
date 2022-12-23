from ragger.backend import RaisePolicy
from application_client.boilerplate_command_sender import CLA, InsType, P1, P2, Errors


# Ensure the app returns an error when a bad CLA is used
def test_bad_cla(backend):
    # Disable raising when trying to unpack an error APDU
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = backend.exchange(cla=CLA + 1, ins=InsType.GET_VERSION)
    assert rapdu.status == Errors.SW_CLA_NOT_SUPPORTED


# Ensure the app returns an error when a bad INS is used
def test_bad_ins(backend):
    # Disable raising when trying to unpack an error APDU
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = backend.exchange(cla=CLA, ins=0xff)
    assert rapdu.status == Errors.SW_INS_NOT_SUPPORTED


# Ensure the app returns an error when a bad P1 or P2 is used
def test_wrong_p1p2(backend):
    # Disable raising when trying to unpack an error APDU
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = backend.exchange(cla=CLA, ins=InsType.GET_VERSION, p1=P1.P1_START + 1, p2=P2.P2_LAST)
    assert rapdu.status == Errors.SW_WRONG_P1P2
    rapdu = backend.exchange(cla=CLA, ins=InsType.GET_VERSION, p1=P1.P1_START, p2=P2.P2_MORE)
    assert rapdu.status == Errors.SW_WRONG_P1P2
    rapdu = backend.exchange(cla=CLA, ins=InsType.GET_APP_NAME, p1=P1.P1_START + 1, p2=P2.P2_LAST)
    assert rapdu.status == Errors.SW_WRONG_P1P2
    rapdu = backend.exchange(cla=CLA, ins=InsType.GET_APP_NAME, p1=P1.P1_START, p2=P2.P2_MORE)
    assert rapdu.status == Errors.SW_WRONG_P1P2


# Ensure the app returns an error when a bad data length is used
def test_wrong_data_length(backend):
    # Disable raising when trying to unpack an error APDU
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    # APDUs must be at least 5 bytes: CLA, INS, P1, P2, Lc.
    rapdu = backend.exchange_raw(b"E0030000")
    assert rapdu.status == Errors.SW_WRONG_DATA_LENGTH
    # APDUs advertises a too long length
    rapdu = backend.exchange_raw(b"E003000005")
    assert rapdu.status == Errors.SW_WRONG_DATA_LENGTH


# Ensure there is no state confusion when trying wrong APDU sequences
def test_invalid_state(backend):
    # Disable raising when trying to unpack an error APDU
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = backend.exchange(cla=CLA,
                             ins=InsType.SIGN_TX,
                             p1=P1.P1_START + 1, # Try to continue a flow instead of start a new one
                             p2=P2.P2_MORE,
                             data=b"abcde") # data is not parsed in this case
    assert rapdu.status == Errors.SW_BAD_STATE
