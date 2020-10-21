import pytest

from ledgercomm import Transport

from apdu_cmd import Command


def pytest_addoption(parser):
    parser.addoption("--hid",
                     action="store_true")


@pytest.fixture(scope="module")
def hid(pytestconfig):
    return pytestconfig.getoption("hid")


@pytest.fixture(scope="module")
def cmd(hid):
    transport = Transport(hid=hid, debug=True)
    command = Command(
        transport=transport,
        debug=True
    )

    yield command

    command.transport.close()
