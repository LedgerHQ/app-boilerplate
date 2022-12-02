import pytest
from pathlib import Path
from ragger import Firmware
from ragger.backend import SpeculosBackend, LedgerCommBackend, LedgerWalletBackend
from ragger.navigator import NanoNavigator
from ragger.utils import app_path_from_app_name

from boilerplate_client.boilerplate_cmd import BoilerplateCommand


# This variable is needed for Speculos only (physical tests need the application to be already installed)
# Adapt this path to your 'tests/elfs' directory
APPS_DIRECTORY = (Path(__file__).parent.parent / "elfs").resolve()

# Adapt this path to the APPNAME in your Makefile
APP_NAME = "boilerplate"

BACKENDS = ["speculos", "ledgercomm", "ledgerwallet"]

FIRMWARES = [Firmware('nanos', '2.1'),
             Firmware('nanox', '2.0.2'),
             Firmware('nanosp', '1.0.3')]

@pytest.fixture(scope="module")
def sw_h_path():
    # path with tests
    conftest_folder_path: Path = Path(__file__).parent
    # sw.h should be in ../../src/sw.h
    sw_h_path = conftest_folder_path.parent / "src" / "sw.h"
    if not sw_h_path.is_file():
        raise FileNotFoundError(f"Can't find sw.h: '{sw_h_path}'")
    return sw_h_path


def pytest_addoption(parser):
    # the default backend is Speculos, this option allows to select another
    parser.addoption("--backend", action="store", default="speculos")
    parser.addoption("--display", action="store_true", default=False)
    parser.addoption("--golden_run", action="store_true", default=False)
    # Enable using --'device' in the pytest command line to restrict testing to specific devices
    for fw in FIRMWARES:
        parser.addoption("--"+fw.device, action="store_true", help="run on nanos only")


@pytest.fixture(scope="session")
def backend_name(pytestconfig):
    return pytestconfig.getoption("backend")


@pytest.fixture(scope="session")
def display(pytestconfig):
    return pytestconfig.getoption("display")


@pytest.fixture(scope="session")
def golden_run(pytestconfig):
    return pytestconfig.getoption("golden_run")


@pytest.fixture
def test_name(request):
    # Get the name of current pytest test
    test_name = request.node.name

    # Remove firmware suffix:
    # -  test_xxx_transaction_ok[nanox 2.0.2]
    # => test_xxx_transaction_ok
    return test_name.split("[")[0]


# Glue to call every test that depends on the firmware once for each required firmware
def pytest_generate_tests(metafunc):
    if "firmware" in metafunc.fixturenames:
        fw_list = []
        ids = []
        # First pass: enable only demanded firmwares
        for fw in FIRMWARES:
            if metafunc.config.getoption(fw.device):
                fw_list.append(fw)
                ids.append(fw.device + " " + fw.version)
        # Second pass if no specific firmware demanded: add them all
        if not fw_list:
            for fw in FIRMWARES:
                fw_list.append(fw)
                ids.append(fw.device + " " + fw.version)
        metafunc.parametrize("firmware", fw_list, ids=ids)


def prepare_speculos_args(firmware: Firmware, display: bool):
    speculos_args = []

    if display:
        speculos_args += ["--display", "qt"]

    app_path = app_path_from_app_name(APPS_DIRECTORY, APP_NAME, firmware.device)

    return ([app_path], {"args": speculos_args})


# Depending on the "--backend" option value, a different backend is
# instantiated, and the tests will either run on Speculos or on a physical
# device depending on the backend
def create_backend(backend_name: str, firmware: Firmware, display: bool):
    if backend_name.lower() == "ledgercomm":
        return LedgerCommBackend(firmware, interface="hid")
    elif backend_name.lower() == "ledgerwallet":
        return LedgerWalletBackend(firmware)
    elif backend_name.lower() == "speculos":
        args, kwargs = prepare_speculos_args(firmware, display)
        return SpeculosBackend(*args, firmware, **kwargs)
    else:
        raise ValueError(f"Backend '{backend_name}' is unknown. Valid backends are: {BACKENDS}")


# This final fixture will return the properly configured backend, to be used in tests
@pytest.fixture
def backend(backend_name, firmware, display):
    with create_backend(backend_name, firmware, display) as b:
        yield b


@pytest.fixture
def cmd(backend):
    yield BoilerplateCommand(client=backend, debug=True)


@pytest.fixture
def navigator(backend, firmware, golden_run):
    if firmware.device.startswith("nano"):
        return NanoNavigator(backend, firmware, golden_run)
    else:
        raise ValueError(f"Device '{firmware.device}' is unsupported.")


@pytest.fixture(autouse=True)
def use_only_on_backend(request, backend):
    if request.node.get_closest_marker('use_on_backend'):
        current_backend = request.node.get_closest_marker('use_on_backend').args[0]
        if current_backend != backend:
            pytest.skip('skipped on this backend: {}'.format(current_backend))


def pytest_configure(config):
    config.addinivalue_line(
        "markers", "use_only_on_backend(backend): skip test if not on the specified backend",
    )
