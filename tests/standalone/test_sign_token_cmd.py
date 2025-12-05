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

# Map token name to address
TOKEN_ADDRESSES_MAPPING = {
    "USDC": TOKEN_USDC,
    "WETH": TOKEN_WETH,
    "DAI": TOKEN_DAI,
    "BTC": TOKEN_BTC,
    "LINK": TOKEN_LINK,
    "UNKNOWN": TOKEN_UNKNOWN,
}

# CAL dynamic token (not in hardcoded database, will be provided via PROVIDE_TOKEN_INFO)
TOKEN_CAL_DYNAMIC = "cafebabedeadbeefcafebabedeadbeefcafebabedeadbeefcafebabedeadbeef"

# We will always use the same path in our tests
STANDARD_PATH = "m/44'/1'/0'/0/0"


# Test signing a token transaction with the first (USDC), middle (WETH) and last (LINK) tokens in the C database
@pytest.mark.parametrize("token_name", ["USDC", "WETH", "LINK"])
def test_sign_token_tx(backend: BackendInterface, scenario_navigator: NavigateWithScenario, test_name: str, token_name: str) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)

    # Set test name for the navigator otherwise all tests will overwrite the same snapshots
    scenario_navigator.test_name = test_name + "_" + token_name

    # First we need to get the public key of the device in order to build the transaction
    rapdu = client.get_public_key(path=STANDARD_PATH)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Create the token transaction with USDC (first token)
    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_ADDRESSES_MAPPING[token_name],
        value=1000000000000,  # 1 USDC with 12 decimals
        memo="USDC transfer"
    ).serialize()

    # Send the sign device instruction for token transaction
    # As it requires on-screen validation, the function is asynchronous.
    # It will yield the result when the navigation is done
    with client.sign_token_tx(path=STANDARD_PATH, transaction=transaction):
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
        client.sign_token_tx_sync(path=STANDARD_PATH, transaction=transaction)

    # Assert that we have received a parsing failure
    assert e.value.status == Errors.SW_TX_PARSING_FAIL
    assert len(e.value.data) == 0


# Test signing a token transaction and refusing it
def test_sign_token_tx_refused(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)

    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_DAI,
        value=1000000000000000,  # 100 DAI with 14 decimals
        memo="This will be refused"
    ).serialize()

    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_token_tx(path=STANDARD_PATH, transaction=transaction):
            scenario_navigator.review_reject()

    # Assert that we have received a refusal
    assert e.value.status == Errors.SW_DENY
    assert len(e.value.data) == 0


# Test signing a token transaction with a long memo (multi-chunk)
def test_sign_token_tx_long_memo(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)

    rapdu = client.get_public_key(path=STANDARD_PATH)
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

    with client.sign_token_tx(path=STANDARD_PATH, transaction=transaction):
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)


    with client.sign_token_tx(path=STANDARD_PATH, transaction=transaction):
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)


# Test providing a valid dynamic token via CAL and signing with it
def test_provide_dynamic_token_valid_new(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    """Test providing a new CAL-signed token and using it to sign a transaction."""
    client = BoilerplateCommandSender(backend)

    # Get public key
    rapdu = client.get_public_key(path=STANDARD_PATH)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Provide dynamic token via CAL (ticker, decimals, 32-byte address)
    token_address = bytes.fromhex(TOKEN_CAL_DYNAMIC)
    rapdu = client.provide_dynamic_token(
        ticker="USDT",
        decimals=6,
        token_address=token_address
    )

    # Now sign a transaction using the dynamically provided token
    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_CAL_DYNAMIC,
        value=500000000,  # 500 USDT with 6 decimals
        memo="Dynamic CAL token"
    ).serialize()

    with client.sign_token_tx(path=STANDARD_PATH, transaction=transaction):
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)


# Test that dynamic token overrides hardcoded database
def test_provide_dynamic_token_override_hardcoded(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    """Test that CAL dynamic token takes priority over hardcoded database."""
    client = BoilerplateCommandSender(backend)

    rapdu = client.get_public_key(path=STANDARD_PATH)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Provide dynamic token with same address as hardcoded USDC but different ticker/decimals
    # This tests that dynamic token lookup has priority
    token_address = bytes.fromhex(TOKEN_USDC)
    rapdu = client.provide_dynamic_token(
        ticker="CUSD",  # Different ticker
        decimals=18,     # Different decimals (hardcoded USDC has 12)
        token_address=token_address
    )

    # Transaction should now use the dynamic token info (CUSD with 18 decimals)
    # We can't easily verify the displayed ticker in unit tests, but we verify
    # that the transaction is accepted and signed successfully
    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_USDC,
        value=1000000000000000000,  # 1 CUSD with 18 decimals (not 12)
        memo="Override test"
    ).serialize()

    with client.sign_token_tx(path=STANDARD_PATH, transaction=transaction):
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)


# Test providing dynamic token with wrong coin type (should fail)
def test_provide_dynamic_token_wrong_coin_type(backend: BackendInterface) -> None:
    """Test that providing a token with wrong SLIP-44 coin type fails."""
    client = BoilerplateCommandSender(backend)

    token_address = bytes.fromhex(TOKEN_CAL_DYNAMIC)

    # Use wrong chain_id (not 0x8001)
    with pytest.raises(ExceptionRAPDU) as e:
        client.provide_dynamic_token(
            ticker="FAIL",
            decimals=6,
            token_address=token_address,
            chain_id=0x003c  # Bitcoin coin type instead of 0x8001
        )

    # Should fail with INVALID_DYNAMIC_TOKEN error
    assert e.value.status == Errors.SW_INVALID_DYNAMIC_TOKEN


# Test providing dynamic token without TUID (malformed TLV)
def test_provide_dynamic_token_missing_tuid(backend: BackendInterface) -> None:
    """Test that providing a token descriptor without TUID fails."""
    client = BoilerplateCommandSender(backend)

    # We need to manually craft a TLV without TUID tag (0x07)
    # This requires accessing backend.exchange directly
    from application_client.tlv import format_tlv
    from application_client.boilerplate_keychain import sign_data, Key
    from application_client.boilerplate_command_sender import CLA, InsType

    # Build TLV without TUID tag (skip tag 0x07)
    tlv_without_sig = b""
    tlv_without_sig += format_tlv(0x01, 0x90)  # STRUCTURE_TYPE
    tlv_without_sig += format_tlv(0x02, 0x01)  # VERSION
    tlv_without_sig += format_tlv(0x03, 0x80000000 | 0x8001)  # CHAIN_ID
    tlv_without_sig += format_tlv(0x04, 0x00)  # SIGNER_ALGO
    tlv_without_sig += format_tlv(0x05, 0x08)  # SIGNER_KEY
    # NO TAG 0x07 (TUID) - intentionally missing
    app_data = b"\x04USDT\x06"  # 4-char ticker + 1 byte decimals
    tlv_without_sig += format_tlv(0x08, app_data)  # APP_DATA

    signature = sign_data(tlv_without_sig, key=Key.DYNAMIC_TOKEN)

    # Insert signature
    tlv_with_sig = b""
    tlv_with_sig += format_tlv(0x01, 0x90)
    tlv_with_sig += format_tlv(0x02, 0x01)
    tlv_with_sig += format_tlv(0x03, 0x80000000 | 0x8001)
    tlv_with_sig += format_tlv(0x04, 0x00)
    tlv_with_sig += format_tlv(0x05, 0x08)
    tlv_with_sig += format_tlv(0x06, signature)
    tlv_with_sig += format_tlv(0x08, app_data)

    with pytest.raises(ExceptionRAPDU) as e:
        backend.exchange(cla=CLA,
                        ins=InsType.PROVIDE_TOKEN_INFO,
                        p1=0x00,
                        p2=0x00,
                        data=tlv_with_sig)

    # Should fail with INVALID_DYNAMIC_TOKEN
    assert e.value.status == Errors.SW_INVALID_DYNAMIC_TOKEN


# Test providing dynamic token with unknown TUID tag (strict validation)
def test_provide_dynamic_token_unknown_tuid_tag(backend: BackendInterface) -> None:
    """Test that TUID with unknown sub-tags is rejected (strict validation)."""
    client = BoilerplateCommandSender(backend)

    from application_client.tlv import format_tlv
    from application_client.boilerplate_keychain import sign_data, Key
    from application_client.boilerplate_command_sender import CLA, InsType

    # Build TUID with unknown tag 0x99 instead of 0x10
    token_address = bytes.fromhex(TOKEN_CAL_DYNAMIC)
    tuid_tlv = format_tlv(0x99, token_address)  # Wrong tag! Should be 0x10

    tlv_without_sig = b""
    tlv_without_sig += format_tlv(0x01, 0x90)
    tlv_without_sig += format_tlv(0x02, 0x01)
    tlv_without_sig += format_tlv(0x03, 0x80000000 | 0x8001)
    tlv_without_sig += format_tlv(0x04, 0x00)
    tlv_without_sig += format_tlv(0x05, 0x08)
    tlv_without_sig += format_tlv(0x07, tuid_tlv)  # TUID with wrong sub-tag
    app_data = b"\x04USDT\x06"
    tlv_without_sig += format_tlv(0x08, app_data)

    signature = sign_data(tlv_without_sig, key=Key.DYNAMIC_TOKEN)

    tlv_with_sig = b""
    tlv_with_sig += format_tlv(0x01, 0x90)
    tlv_with_sig += format_tlv(0x02, 0x01)
    tlv_with_sig += format_tlv(0x03, 0x80000000 | 0x8001)
    tlv_with_sig += format_tlv(0x04, 0x00)
    tlv_with_sig += format_tlv(0x05, 0x08)
    tlv_with_sig += format_tlv(0x06, signature)
    tlv_with_sig += format_tlv(0x07, tuid_tlv)
    tlv_with_sig += format_tlv(0x08, app_data)

    with pytest.raises(ExceptionRAPDU) as e:
        backend.exchange(cla=CLA,
                        ins=InsType.PROVIDE_TOKEN_INFO,
                        p1=0x00,
                        p2=0x00,
                        data=tlv_with_sig)

    # Should fail with INVALID_DYNAMIC_TOKEN due to strict TUID validation
    assert e.value.status == Errors.SW_INVALID_DYNAMIC_TOKEN


# Test that dynamic token persists across regular transactions
def test_provide_dynamic_token_persist_across_regular_tx(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    """Test that dynamic token info persists when signing a regular (non-token) transaction."""
    client = BoilerplateCommandSender(backend)

    rapdu = client.get_public_key(path=STANDARD_PATH)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Provide dynamic token
    token_address = bytes.fromhex(TOKEN_CAL_DYNAMIC)
    rapdu = client.provide_dynamic_token(
        ticker="USDT",
        decimals=6,
        token_address=token_address
    )

    # Sign a regular transaction (not a token transaction)
    from application_client.boilerplate_transaction import Transaction
    regular_tx = Transaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        value=100000000000000,
        memo="Regular transaction between tokens"
    ).serialize()

    with client.sign_tx(path=STANDARD_PATH, transaction=regular_tx):
        # We need to ensure the snapshots name do not conflict
        scenario_navigator.test_name += "/tx_1"
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, regular_tx)

    # Now sign another token transaction with the same dynamic token
    # If persistence works (like Solana), this should succeed without re-providing
    token_tx = TokenTransaction(
        nonce=2,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_CAL_DYNAMIC,
        value=1000000,  # 1 USDT
        memo="After regular tx"
    ).serialize()

    with client.sign_token_tx(path=STANDARD_PATH, transaction=token_tx):
        # We need to ensure the snapshots name do not conflict
        scenario_navigator.test_name += "/tx_2"
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, token_tx)


# Test providing multiple different dynamic tokens (only last one should persist)
def test_provide_dynamic_token_multiple_sequential(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    """Test that providing multiple dynamic tokens replaces the previous one."""
    client = BoilerplateCommandSender(backend)

    rapdu = client.get_public_key(path=STANDARD_PATH)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Provide first dynamic token
    token_address_1 = bytes.fromhex(TOKEN_CAL_DYNAMIC)
    client.provide_dynamic_token(
        ticker="TOK1",
        decimals=6,
        token_address=token_address_1
    )

    # Provide second dynamic token with different address (should replace first)
    token_address_2 = bytes.fromhex("1111111111111111111111111111111111111111111111111111111111111111")
    client.provide_dynamic_token(
        ticker="TOK2",
        decimals=8,
        token_address=token_address_2
    )

    # Transaction with second token address should work
    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address="1111111111111111111111111111111111111111111111111111111111111111",
        value=100000000,  # 1 TOK2
        memo="Second token"
    ).serialize()

    with client.sign_token_tx(path=STANDARD_PATH, transaction=transaction):
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)

    # Transaction with first token address should now fail (unknown token)
    transaction_old = TokenTransaction(
        nonce=2,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_CAL_DYNAMIC,
        value=1000000,
        memo="First token (should fail)"
    ).serialize()

    with pytest.raises(ExceptionRAPDU) as e:
        client.sign_token_tx_sync(path=STANDARD_PATH, transaction=transaction_old)

    assert e.value.status == Errors.SW_TX_PARSING_FAIL


# Test dynamic token with maximum ticker length
def test_provide_dynamic_token_max_ticker_length(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    """Test providing a dynamic token with maximum ticker length (SDK validates ≤50 chars)."""
    client = BoilerplateCommandSender(backend)

    rapdu = client.get_public_key(path=STANDARD_PATH)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Provide dynamic token with long ticker (but within SDK limit)
    token_address = bytes.fromhex(TOKEN_CAL_DYNAMIC)
    long_ticker = "A" * 12  # 12 chars - reasonable length that fits in MAX_TICKER_SIZE

    rapdu = client.provide_dynamic_token(
        ticker=long_ticker,
        decimals=6,
        token_address=token_address
    )
    assert rapdu.status == 0x9000

    # Sign transaction to verify it works
    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_CAL_DYNAMIC,
        value=1000000,
        memo="Long ticker test"
    ).serialize()

    with client.sign_token_tx(path=STANDARD_PATH, transaction=transaction):
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)


# Test providing dynamic token with zero decimals
def test_provide_dynamic_token_zero_decimals(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    """Test providing a dynamic token with 0 decimals (valid edge case)."""
    client = BoilerplateCommandSender(backend)

    rapdu = client.get_public_key(path=STANDARD_PATH)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    # Provide dynamic token with 0 decimals (like some NFT tokens)
    token_address = bytes.fromhex(TOKEN_CAL_DYNAMIC)
    rapdu = client.provide_dynamic_token(
        ticker="NFT",
        decimals=0,
        token_address=token_address
    )
    assert rapdu.status == 0x9000

    # Sign transaction with integer value (no decimals)
    transaction = TokenTransaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        token_address=TOKEN_CAL_DYNAMIC,
        value=1,  # 1 NFT
        memo="NFT transfer"
    ).serialize()

    with client.sign_token_tx(path=STANDARD_PATH, transaction=transaction):
        scenario_navigator.review_approve()

    response = client.get_async_response().data
    _, der_sig, _ = unpack_sign_tx_response(response)
    assert check_signature_validity(public_key, der_sig, transaction)


# Test providing dynamic token with TUID wrong size
def test_provide_dynamic_token_wrong_tuid_size(backend: BackendInterface) -> None:
    """Test that TUID with wrong address size is rejected."""
    from application_client.tlv import format_tlv
    from application_client.boilerplate_keychain import sign_data, Key
    from application_client.boilerplate_command_sender import CLA, InsType, Errors

    # Build TUID with 20-byte address instead of 32 bytes
    wrong_size_address = b"\x00" * 20  # Wrong size!
    tuid_tlv = format_tlv(0x10, wrong_size_address)

    payload = format_tlv(0x01, 0x90)  # STRUCTURE_TYPE
    payload += format_tlv(0x02, 0x01)  # VERSION
    payload += format_tlv(0x03, 0x8001)  # COIN_TYPE
    payload += format_tlv(0x04, "Boilerplate")  # APP
    payload += format_tlv(0x05, "FAIL")  # TICKER
    payload += format_tlv(0x06, 6)  # MAGNITUDE
    payload += format_tlv(0x07, tuid_tlv)  # TUID with wrong size

    signature = sign_data(payload, key=Key.DYNAMIC_TOKEN)
    payload += format_tlv(0x08, signature)  # SIGNATURE

    with pytest.raises(ExceptionRAPDU) as e:
        backend.exchange(cla=CLA,
                        ins=InsType.PROVIDE_TOKEN_INFO,
                        p1=0x00,
                        p2=0x00,
                        data=payload)

    # Should fail with INVALID_DYNAMIC_TOKEN
    assert e.value.status == Errors.SW_INVALID_DYNAMIC_TOKEN
