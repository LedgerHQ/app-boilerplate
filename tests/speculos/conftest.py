from collections import namedtuple
from pathlib import Path

import pytest

from speculos.client import SpeculosClient

from boilerplate_client.boilerplate_cmd import BoilerplateCommand


SCRIPT_DIR = Path(__file__).absolute().parent
API_URL = "http://127.0.0.1:5000"


def pytest_addoption(parser):
    parser.addoption("--model",
                     action="store", 
                     default="nanos")
    parser.addoption("--sdk",
                     action="store",
                     default="2.1")


@pytest.fixture(scope="session")
def model(pytestconfig):
    return pytestconfig.getoption("model")


@pytest.fixture(scope="session")
def sdk(pytestconfig):
    return pytestconfig.getoption("sdk")

@pytest.fixture(scope="module")
def sw_h_path():
    # path with tests
    conftest_folder_path: Path = Path(__file__).parent
    # sw.h should be in ../../src/sw.h
    sw_h_path = conftest_folder_path.parent.parent / "src" / "sw.h"

    if not sw_h_path.is_file():
        raise FileNotFoundError(f"Can't find sw.h: '{sw_h_path}'")

    return sw_h_path


@pytest.fixture
def client(model, sdk):
    file_path = SCRIPT_DIR.parent.parent / "bin" / "app.elf"
    args = ['--model', model, '--sdk', sdk]
    with SpeculosClient(app=str(file_path), args=args) as client:
        yield client


@pytest.fixture
def cmd(client):
    yield BoilerplateCommand(
        client=client,
        debug=True
    )
