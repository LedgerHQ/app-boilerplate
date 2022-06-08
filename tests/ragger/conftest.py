from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Union

import pytest

from ragger import Firmware
from ragger.backend import SpeculosBackend, LedgerCommBackend, LedgerWalletBackend

from boilerplate_client.boilerplate_cmd import BoilerplateCommand


@dataclass(frozen=True)
class Application:
    path: Path
    firmware: Firmware

APPLICATION_DIRECTORY = (Path(__file__).parent.parent.parent / "elfs").resolve()
BACKENDS = ["speculos", "ledgercomm", "ledgerwallet"]
APPS = [
    Application(APPLICATION_DIRECTORY / 'boilerplate_nanos.elf',
                Firmware('nanos', '2.1')),
    Application(APPLICATION_DIRECTORY / 'boilerplate_nanox.elf',
                Firmware('nanox', '2.0.2')),
    Application(APPLICATION_DIRECTORY / 'boilerplate_nanosp.elf',
                Firmware('nanosp', '1.0.3'))
]
SCRIPT_DIR = Path(__file__).absolute().parent


@pytest.fixture(scope="module")
def sw_h_path():
    # path with tests
    conftest_folder_path: Path = Path(__file__).parent
    # sw.h should be in ../../src/sw.h
    sw_h_path = conftest_folder_path.parent.parent / "src" / "sw.h"

    if not sw_h_path.is_file():
        raise FileNotFoundError(f"Can't find sw.h: '{sw_h_path}'")

    return sw_h_path


@pytest.fixture(params=APPS)
def exchange(request):
    return request.param


def prepare_speculos_args(exchange):
    current_device = exchange.firmware.device
    assert APPLICATION_DIRECTORY.is_dir(), \
        f"{APPLICATION_DIRECTORY} is not a directory"
    application = None
    speculos_args = ["--model", current_device, "--sdk", exchange.firmware.version]
    original_size = len(speculos_args)
    application = exchange.path
    assert exchange.path.is_file(), f"{exchange.path} must exist"
    return ([application], {"args": speculos_args})


def pytest_addoption(parser):
    parser.addoption("--backend", action="store", default="speculos")


@pytest.fixture(scope="session")
def backend(pytestconfig):
    return pytestconfig.getoption("backend")


def create_backend(backend: bool, exchange: Application):
    if backend.lower() == "ledgercomm":
        return LedgerCommBackend(exchange.firmware, interface="hid")
    elif backend.lower() == "ledgerwallet":
        return LedgerWalletBackend(exchange.firmware)
    elif backend.lower() == "speculos":
        args, kwargs = prepare_speculos_args(exchange)
        return SpeculosBackend(*args, exchange.firmware, **kwargs)
    else:
        raise ValueError(f"Backend '{backend}' is unknown. Valid backends are: {BACKENDS}")


@pytest.fixture
def client(backend, exchange):
    with create_backend(backend, exchange) as b:
        yield b


@pytest.fixture
def client_no_raise(backend, exchange):
    with create_backend(backend, exchange, raises=False) as b:
        yield b


@pytest.fixture
def cmd(client):
    yield BoilerplateCommand(client=client, debug=True)


@pytest.fixture(autouse=True)
def use_only_on_backend(request, backend: Union[str, Iterable]):
    if not isinstance(backend, Iterable):
        backend = [backend]
    if request.node.get_closest_marker('use_on_backend'):
        current_backend = request.node.get_closest_marker('use_on_backend').args[0]
        if current_backend not in backend:
            pytest.skip('skipped on this backend: {}'.format(current_backend))


def pytest_configure(config):
  config.addinivalue_line(
        "markers", "use_only_on_backend(backend): skip test if not on the specified backend",
  )
