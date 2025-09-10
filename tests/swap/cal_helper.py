from ledger_app_clients.exchange.cal_helper import CurrencyConfiguration
from ragger.bip import pack_derivation_path
from ragger.utils import create_currency_config
from application_client.boilerplate_currency_utils import BOL_PATH

# Define a configuration for each currency used in our tests: native coins and tokens

# BOL token currency definition
BOL_CONF = create_currency_config("BOL", "Boilerplate", sub_coin_config=None)
# Serialized derivation path for the Boilerplate app
BOL_PACKED_DERIVATION_PATH = pack_derivation_path(BOL_PATH)
# Coin configuration mock as stored in CAL for the SWAP feature
BOL_CURRENCY_CONFIGURATION = CurrencyConfiguration(ticker="BOL", conf=BOL_CONF, packed_derivation_path=BOL_PACKED_DERIVATION_PATH)
