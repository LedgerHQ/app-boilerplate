###########################
### CONFIGURATION START ###
###########################

# You can configure optional parameters by overriding the value of ragger.configuration.OPTIONAL_CONFIGURATION
# Please refer to ragger/conftest/configuration.py for their descriptions and accepted values

#########################
### CONFIGURATION END ###
#########################

# Record the tests tagged @pytest.mark.flow for doc/FLOWS.md, see generate_flows.py
from .flow_recorder import (  # noqa: F401
    _record_flow,
    pytest_addoption,
    pytest_configure,
    pytest_runtest_makereport,
)

# Pull all features from the base ragger conftest using the overridden configuration
pytest_plugins = ("ragger.conftest.base_conftest",)
