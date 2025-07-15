import pytest

from pathlib import Path

from ragger.conftest import configuration

from ledger_app_clients.exchange.navigation_helper import ExchangeNavigationHelper

###########################
### CONFIGURATION START ###
###########################

# You can configure optional parameters by overriding the value of ragger.configuration.OPTIONAL_CONFIGURATION
# Please refer to ragger/conftest/configuration.py for their descriptions and accepted values

configuration.OPTIONAL.BACKEND_SCOPE = "class"
configuration.OPTIONAL.MAIN_APP_DIR = "tests/swap/.test_dependencies/main"
configuration.OPTIONAL.SIDELOADED_APPS_DIR = "tests/swap/.test_dependencies/libraries/"

#########################
### CONFIGURATION END ###
#########################

# Pull all features from the base ragger conftest using the overridden configuration
pytest_plugins = ("ragger.conftest.base_conftest", )

@pytest.fixture(scope="session")
def snapshots_path():
    """
    This fixture provides the default path for screenshots.
    It is used in the ExchangeNavigationHelper.
    """
    # Use the current file's directory as the base path
    return Path(__file__).parent.resolve()

@pytest.fixture(scope="function")
def exchange_navigation_helper(backend, navigator, snapshots_path, test_name):
    return ExchangeNavigationHelper(backend=backend, navigator=navigator, snapshots_path=snapshots_path, test_name=test_name)

# Pytest is trying to do "smart" stuff and reorders tests using parametrize by alphabetical order of parameter
# This breaks the backend scope optim. We disable this
def pytest_collection_modifyitems(config, items):
    def param_part(item):
        # Sort by node id as usual
        return item.nodeid

    # re-order the items using the param_part function as key
    items[:] = sorted(items, key=param_part)
