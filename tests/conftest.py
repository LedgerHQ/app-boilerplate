from pathlib import Path
from typing import List, Optional
import pytest
from ragger.firmware import Firmware
from ragger.conftest.base_conftest import prepare_speculos_args
from ragger.backend import SpeculosBackend

###########################
### CONFIGURATION START ###
###########################

# You can configure optional parameters by overriding the value
# of ragger.configuration.OPTIONAL_CONFIGURATION
# Please refer to ragger/conftest/configuration.py for their descriptions and accepted values

#########################
### CONFIGURATION END ###
#########################

# Pull all features from the base ragger conftest using the overridden configuration
pytest_plugins = ("ragger.conftest.base_conftest", )

@pytest.fixture(scope="function")
def get_2_backends(
        root_pytest_dir: Path,
        firmware: Firmware,
        display: bool,
        log_apdu_file: Optional[Path],
        cli_user_seed: str,
        additional_speculos_arguments: List[str]):

    b = []
    additional_speculos_arguments_loc = \
        [additional_speculos_arguments, \
        additional_speculos_arguments + ["--load-nvram"]]

    for i in range(2):
        main_app_path, speculos_args = prepare_speculos_args(root_pytest_dir, firmware, display,
                                                         cli_user_seed,
                                                         additional_speculos_arguments_loc[i])
        b.append(SpeculosBackend(main_app_path,
                               firmware=firmware,
                               log_apdu_file=log_apdu_file,
                               **speculos_args))
    yield b[0], b[1]
