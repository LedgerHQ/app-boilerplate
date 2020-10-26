import pytest

from ledgercomm import Transport

from boilerplate_cmd import BoilerplateCommand


def pytest_addoption(parser):
    parser.addoption("--hid",
                     action="store_true")


@pytest.fixture(scope="module")
def hid(pytestconfig):
    return pytestconfig.getoption("hid")


@pytest.fixture(scope="module")
def cmd(hid):
    transport = (Transport(interface="hid", debug=True)
                 if hid else Transport(interface="tcp", debug=True))
    command = BoilerplateCommand(
        transport=transport,
        debug=True
    )

    yield command

    command.transport.close()
