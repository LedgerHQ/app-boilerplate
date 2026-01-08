from ragger.conftest import configuration

###########################
### CONFIGURATION START ###
###########################

# You can configure optional parameters by overriding the value of ragger.configuration.OPTIONAL_CONFIGURATION
# Please refer to ragger/conftest/configuration.py for their descriptions and accepted values

# Ragger tests are supposed to run without any hardcoded seed / mnemonic.
# However, for debugging, we might want to fix the seed occasionally
# to a value corresponding to the unit test fixtures.
configuration.OPTIONAL.CUSTOM_SEED = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"

#########################
### CONFIGURATION END ###
#########################

# Pull all features from the base ragger conftest using the overridden configuration
pytest_plugins = ("ragger.conftest.base_conftest", )
