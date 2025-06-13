from enum import IntEnum

from ragger.bip import pack_derivation_path
from ragger.utils import create_currency_config, RAPDU

BOL_PATH = "m/44'/1'/0'/0/0"
BOL_CONF = create_currency_config("BOL", "Boilerplate")
BOL_PACKED_DERIVATION_PATH = pack_derivation_path(BOL_PATH)
