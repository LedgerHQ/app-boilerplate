
from ledger_app_clients.exchange.cal_helper import CurrencyConfiguration 

from .boilerplate import BOL_PACKED_DERIVATION_PATH, BOL_CONF

# Define a configuration for each currency used in our tests: native coins and tokens

BOL_CURRENCY_CONFIGURATION = CurrencyConfiguration(ticker="BOL", conf=BOL_CONF, packed_derivation_path=BOL_PACKED_DERIVATION_PATH)
