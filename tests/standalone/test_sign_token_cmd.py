import pytest

from ragger.backend.interface import BackendInterface
from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario

from application_client.boilerplate_transaction import TokenTransaction
from application_client.boilerplate_command_sender import BoilerplateCommandSender, Errors
from application_client.boilerplate_response_unpacker import unpack_get_public_key_response, unpack_sign_tx_response
from .utils import check_signature_validity

# In this tests we check the behavior of the device when asked to sign a token transaction

# Token addresses from the hardcoded database in src/token/token_db.c
# Token 1: USDC with 12 decimals (first token)
TOKEN_USDC = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

# Token 2: WETH with 14 decimals
TOKEN_WETH = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"

# Token 3: DAI with 14 decimals
TOKEN_DAI = "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899"

# Token 4: BTC with 12 decimals
TOKEN_BTC = "112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00"

# Token 5: LINK with 14 decimals (last token)
TOKEN_LINK = "ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100"

# Unknown token (not in database)
TOKEN_UNKNOWN = "0000000000000000000000000000000000000000000000000000000000000000"


# Test signing a token transaction with the first token in the database (USDC)
def test_sign_token_tx_first_token(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    # The path used for this entire test
    path: str = "m/44'/1'/0'/0/0"

    # First we need to get the public key of the device in order to build the transaction
    rapdu = client.get_public_key(path=path)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Create the token transaction with USDC (first token)
    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_USDC,
        value=1000000000000,  # 1 USDC with 12 decimals
        memo="USDC transfer"
    ).serialize()

    # Send the sign device instruction for token transaction
    # As it requires on-screen validation, the function is asynchronous.
    # It will yield the result when the navigation is done
    with client.sign_token_tx(path=path, transaction=transaction):
        # Validate the on-screen request by performing the navigation appropriate for this device
        scenario_navigator.review_approve()

    # The device has yielded the result, parse it and ensure that the signature is correct
    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)


# Test signing a token transaction with the last token in the database (LINK)
def test_sign_token_tx_last_token(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    # The path used for this entire test
    path: str = "m/44'/1'/0'/0/0"

    # First we need to get the public key of the device in order to build the transaction
    rapdu = client.get_public_key(path=path)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Create the token transaction with LINK (last token)
    transaction = TokenTransaction(
        nonce=2,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_LINK,
        value=50000000000000,  # 5 LINK with 14 decimals
        memo="LINK payment"
    ).serialize()

    # Send the sign device instruction for token transaction
    with client.sign_token_tx(path=path, transaction=transaction):
        # Validate the on-screen request by performing the navigation appropriate for this device
        scenario_navigator.review_approve()

    # The device has yielded the result, parse it and ensure that the signature is correct
    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)


# Test signing a token transaction with a token not in the database
# This should fail with TX_PARSING_FAIL error
def test_sign_token_tx_unknown_token(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    # The path used for this entire test
    path: str = "m/44'/1'/0'/0/0"

    # Create the token transaction with an unknown token address
    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_UNKNOWN,
        value=1000,
        memo="Unknown token"
    ).serialize()

    # This should fail because the token is not in the database
    with pytest.raises(ExceptionRAPDU) as e:
        client.sign_token_tx_sync(path=path, transaction=transaction)

    # Assert that we have received a parsing failure
    assert e.value.status == Errors.SW_TX_PARSING_FAIL
    assert len(e.value.data) == 0


# Test signing a token transaction with WETH (middle of the database)
def test_sign_token_tx_middle_token(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    path: str = "m/44'/1'/0'/0/0"

    rapdu = client.get_public_key(path=path)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Create the token transaction with WETH
    transaction = TokenTransaction(
        nonce=5,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_WETH,
        value=200000000000000,  # 2 WETH with 14 decimals
        memo="WETH swap"
    ).serialize()

    with client.sign_token_tx(path=path, transaction=transaction):
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)


# Test signing a token transaction and refusing it
def test_sign_token_tx_refused(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    path: str = "m/44'/1'/0'/0/0"

    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_DAI,
        value=1000000000000000,  # 100 DAI with 14 decimals
        memo="This will be refused"
    ).serialize()

    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_token_tx(path=path, transaction=transaction):
            scenario_navigator.review_reject()

    # Assert that we have received a refusal
    assert e.value.status == Errors.SW_DENY
    assert len(e.value.data) == 0


# Test signing a token transaction with a long memo (multi-chunk)
def test_sign_token_tx_long_memo(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    path: str = "m/44'/1'/0'/0/0"

    rapdu = client.get_public_key(path=path)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Create a transaction with a long memo to force multi-chunk transmission
    transaction = TokenTransaction(
        nonce=10,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_BTC,
        value=500000000000,  # 0.5 BTC with 12 decimals
        memo=("This is a very long memo for a token transaction. "
              "It will force the app client to send the serialized transaction in chunks. "
              "As the maximum chunk size is 255 bytes we will make this memo greater than 255 characters. "
              "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed non risus. Suspendisse lectus tortor.")
    ).serialize()

    with client.sign_token_tx(path=path, transaction=transaction):
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)


# Test signing a token transaction with zero value
def test_sign_token_tx_zero_value(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    path: str = "m/44'/1'/0'/0/0"

    rapdu = client.get_public_key(path=path)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Create the token transaction with zero value
    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_USDC,
        value=0,
        memo="Zero value test"
    ).serialize()

    with client.sign_token_tx(path=path, transaction=transaction):
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)
