from enum import IntEnum

from ragger.bip import pack_derivation_path
from ragger.utils import create_currency_config, RAPDU

# this file aims to define the 'BOL' currency
# managed by the Boilerplate app

# Derivation path used for the Boilerplate app
BOL_PATH = "m/44'/1'/0'/0/0"
# BOL token currency definition
BOL_CONF = create_currency_config("BOL", "Boilerplate")
# Serialized derivation path for the Boilerplate app
BOL_PACKED_DERIVATION_PATH = pack_derivation_path(BOL_PATH)
