from collections import namedtuple
from pathlib import Path

import pytest

from speculos.client import SpeculosClient

from boilerplate_client.boilerplate_cmd import BoilerplateCommand


SCRIPT_DIR = Path(__file__).absolute().parent
API_URL = "http://127.0.0.1:5000"


@pytest.fixture(scope="module")
def sw_h_path():
    # path with tests
    conftest_folder_path: Path = Path(__file__).parent
    # sw.h should be in ../../src/sw.h
    sw_h_path = conftest_folder_path.parent.parent / "src" / "sw.h"

    if not sw_h_path.is_file():
        raise FileNotFoundError(f"Can't find sw.h: '{sw_h_path}'")

    return sw_h_path


@pytest.fixture(scope="session")
def client():
    file_path = SCRIPT_DIR.parent.parent / "bin" / "app.elf"
    args = ['--model', 'nanos', '--sdk', '2.1']
    with SpeculosClient(app=str(file_path), args=args) as client:
        yield client


@pytest.fixture(scope="session")
def cmd(client):
    yield BoilerplateCommand(
        client=client,
        debug=True
    )
