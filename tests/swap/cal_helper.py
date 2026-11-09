from ledger_app_clients.exchange.cal_helper import CurrencyConfiguration
from ragger.bip import pack_derivation_path
from ragger.utils import create_currency_config
from ragger.utils.misc import prefix_with_len

from application_client.boilerplate_currency_utils import BOL_PATH

# Define a configuration for each currency used in our tests: native coins and tokens

# BOL token currency definition
BOL_CONF = create_currency_config("BOL", "Boilerplate", sub_coin_config=None)
# Serialized derivation path for the Boilerplate app
BOL_PACKED_DERIVATION_PATH = pack_derivation_path(BOL_PATH)
# Coin configuration mock as stored in CAL for the SWAP feature
BOL_CURRENCY_CONFIGURATION = CurrencyConfiguration(
    ticker="BOL", conf=BOL_CONF, packed_derivation_path=BOL_PACKED_DERIVATION_PATH
)

# BOL USDC token configuration - USDC with 12 decimals as defined in token_db.c
BOL_USDC_CONF = create_currency_config("USDC", "Boilerplate", sub_coin_config=("USDC", 12))
BOL_USDC_PACKED_DERIVATION_PATH = BOL_PACKED_DERIVATION_PATH
BOL_USDC_CURRENCY_CONFIGURATION = CurrencyConfiguration(
    ticker="USDC",
    conf=BOL_USDC_CONF,
    packed_derivation_path=BOL_USDC_PACKED_DERIVATION_PATH,
)

# BOL Dynamic USDT token configuration - USDT with 6 decimals provided via CAL
BOL_DYNAMIC_USDT_CONF = create_currency_config("USDT", "Boilerplate", sub_coin_config=("USDT", 6))
BOL_DYNAMIC_USDT_PACKED_DERIVATION_PATH = BOL_PACKED_DERIVATION_PATH
BOL_DYNAMIC_USDT_CURRENCY_CONFIGURATION = CurrencyConfiguration(
    ticker="USDT",
    conf=BOL_DYNAMIC_USDT_CONF,
    packed_derivation_path=BOL_DYNAMIC_USDT_PACKED_DERIVATION_PATH,
)

# Token addresses
TOKEN_DYNAMIC_USDT_ADDRESS = "cafebabedeadbeefcafebabedeadbeefcafebabedeadbeefcafebabedeadbeef"
TOKEN_HARDCODED_USDC_ADDRESS = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"


def create_token_currency_config(ticker: str, decimals: int, token_address: str) -> bytes:
    """Token currency configuration whose sub-configuration ends with the token contract address."""
    sub_config = prefix_with_len(ticker.encode()) + decimals.to_bytes(1, byteorder="big")
    sub_config += bytes.fromhex(token_address)
    coin_config = b""
    for element in [ticker.encode(), b"Boilerplate", sub_config]:
        coin_config += prefix_with_len(element)
    return coin_config


# BOL USDC token configuration carrying the token contract address
BOL_USDC_WITH_ADDRESS_CURRENCY_CONFIGURATION = CurrencyConfiguration(
    ticker="USDC",
    conf=create_token_currency_config("USDC", 12, TOKEN_HARDCODED_USDC_ADDRESS),
    packed_derivation_path=BOL_USDC_PACKED_DERIVATION_PATH,
)
