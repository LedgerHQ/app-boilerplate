from ragger.backend import RaisePolicy

from boilerplate_client.errors import ERRORS


def test_bad_cla(backend):
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = backend.exchange(cla=0xa0,  # 0xa0 instead of 0xe0
                     ins=0x03)
    assert rapdu.status == ERRORS["SW_CLA_NOT_SUPPORTED"]

def test_bad_ins(backend):
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = backend.exchange(cla=0xe0, ins=0xff)  # bad INS
    assert rapdu.status == ERRORS["SW_INS_NOT_SUPPORTED"]

def test_wrong_p1p2(backend):
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = backend.exchange(cla=0xe0,
                     ins=0x03,
                     p1=0x01)  # 0x01 instead of 0x00
    assert rapdu.status == ERRORS["SW_WRONG_P1P2"]

def test_wrong_data_length(backend):
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    # APDUs must be at least 5 bytes: CLA, INS, P1, P2, Lc.
    rapdu = backend.exchange_raw(b"E000")
    assert rapdu.status == ERRORS["SW_WRONG_DATA_LENGTH"]
