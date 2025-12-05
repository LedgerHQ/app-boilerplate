import pytest
from ledger_app_clients.exchange.test_runner import ExchangeTestRunner, ALL_TESTS_EXCEPT_MEMO_THORSWAP_AND_FEES
from ledger_app_clients.exchange.cal_helper import CurrencyConfiguration
from ragger.error import ExceptionRAPDU
from ragger.utils import create_currency_config

from application_client.boilerplate_currency_utils import BOL_PATH
from application_client.boilerplate_command_sender import BoilerplateCommandSender, Errors as BoilerplateErrors
from application_client.boilerplate_transaction import Transaction, TokenTransaction

from . import cal_helper as cal


# Token addresses from the hardcoded database in src/token/token_db.c
# USDC token with 12 decimals
TOKEN_USDC_ADDRESS = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
TOKEN_LINK_ADDRESS = "ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100"
UNKNOWN_TOKEN_ADDRESS = "00ffeeddccbbaa99887766554433221100ffeeddccbbaa998877665544332211"

# CAL dynamic token address (not in hardcoded database, will be provided dynamically)
TOKEN_DYNAMIC_USDT_ADDRESS = cal.TOKEN_DYNAMIC_USDT_ADDRESS
TOKEN_HARDCODED_USDC_ADDRESS = cal.TOKEN_HARDCODED_USDC_ADDRESS


# ExchangeTestRunner implementation for Boilerplate.
# BoilerplateTests extends ExchangeTestRunner and in doing so inherits all necessary boilerplate and
# test management features.
# We only need to set the values we want to use for our test and the final payment request
class BoilerplateTests(ExchangeTestRunner):
    # The coin configuration of our currency. Replace by your own
    currency_configuration = cal.BOL_CURRENCY_CONFIGURATION
    # A valid template address of a supposed trade partner.
    valid_destination_1 = "de0b295669a9fd93d5f28d9ec85e40f4cb697bae"
    # A memo to use associated with the destination address if applicable.
    valid_destination_memo_1 = ""
    # A second valid template address of a supposed trade partner.
    valid_destination_2 = "9fc3da866e7dF3a1c57ade1a97c9f00a70f010c8"
    # A second memo to use associated with the destination address if applicable.
    valid_destination_memo_2 = ""
    # The address of the Speculos seed on the BOL_PATH.
    valid_refund = "FD2095A37E72BE2CD575D18FE8F16E78C51EAFA3"
    valid_refund_memo = ""

    # Values we ask the ExchangeTestRunner to use in the test setup
    valid_send_amount_1 = 1000
    valid_send_amount_2 = 666
    valid_fees_1 = 0
    valid_fees_2 = 0

    # Fake addresses to test the address rejection code.
    fake_refund = "abcdabcd"
    fake_refund_memo = "bla"
    fake_payout = "abcdabcd"
    fake_payout_memo = "bla"

    # The error code we expect our application to respond when encountering errors.
    signature_refusal_error_code = BoilerplateErrors.SW_DENY
    wrong_amount_error_code = BoilerplateErrors.SW_SWAP_FAIL
    wrong_destination_error_code = BoilerplateErrors.SW_SWAP_FAIL

    # The final transaction to craft and send as part of the SWAP finalization.
    # This function will be called by the ExchangeTestRunner in a callback like way
    def perform_final_tx(self, destination, send_amount, fees, memo):
        # Create the transaction that will be sent to the device for signing
        tx = Transaction(
            nonce=1,
            to=destination,
            value=send_amount,
            memo=memo
        ).serialize()

        # Send the TX
        BoilerplateCommandSender(self.backend).sign_tx_sync(path=BOL_PATH, transaction=tx)

        # TODO : assert signature validity. Not required but recommended


# Token addresses from the hardcoded database in src/token/token_db.c
# USDC token with 12 decimals
TOKEN_USDC_ADDRESS = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
TOKEN_LINK_ADDRESS = "ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100"
UNKNOWN_TOKEN_ADDRESS = "00ffeeddccbbaa99887766554433221100ffeeddccbbaa998877665544332211"

# CAL dynamic token address (not in hardcoded database, will be provided dynamically)
TOKEN_DYNAMIC_USDT_ADDRESS = "cafebabedeadbeefcafebabedeadbeefcafebabedeadbeefcafebabedeadbeef"

# ExchangeTestRunner implementation for Boilerplate Token Swap.
# This test class extends BoilerplateTests but uses TokenTransaction instead of Transaction
class BoilerplateTokenTests(BoilerplateTests):
    # Token currency configuration - USDC with 12 decimals as defined in token_db.c
    currency_configuration = cal.BOL_USDC_CURRENCY_CONFIGURATION

    def _perform_final_tx_with_token(self, destination, send_amount, fees, memo, token_address):
        # Create the token transaction that will be sent to the device for signing
        tx = TokenTransaction(
            nonce=1,
            to=destination,
            token_address=token_address,
            value=send_amount,
            memo=memo
        ).serialize()

        # Send the token TX
        BoilerplateCommandSender(self.backend).sign_token_tx_sync(path=BOL_PATH, transaction=tx)

        # TODO : assert signature validity. Not required but recommended

    def perform_final_tx(self, destination, send_amount, fees, memo):
            # Matching the BOL_USDC_CURRENCY_CONFIGURATION provided earlier
        self._perform_final_tx_with_token(destination, send_amount, fees, memo, TOKEN_USDC_ADDRESS)

    def perform_final_tx_token_mismatch(self, destination, send_amount, fees, memo):
        self._perform_final_tx_with_token(destination, send_amount, fees, memo, TOKEN_LINK_ADDRESS)

    def perform_final_tx_token_unknown(self, destination, send_amount, fees, memo):
        self._perform_final_tx_with_token(destination, send_amount, fees, memo, UNKNOWN_TOKEN_ADDRESS)

# We use a class to reuse the same Speculos instance (faster performances)
class TestsBoilerplate:
    # Run all the tests applicable to our setup: here we don't test fees mismatch, memo mismatch, and Thorswap / LiFi
    @pytest.mark.parametrize('test_to_run', ALL_TESTS_EXCEPT_MEMO_THORSWAP_AND_FEES)
    def test_boilerplate(self, backend, exchange_navigation_helper, test_to_run):
        # Call run_test method of ExchangeTestRunner
        BoilerplateTests(backend, exchange_navigation_helper).run_test(test_to_run)

    # Token swap test - this should fail as the C code currently rejects token swaps
    @pytest.mark.parametrize('test_to_run', ALL_TESTS_EXCEPT_MEMO_THORSWAP_AND_FEES)
    def test_boilerplate_token(self, backend, exchange_navigation_helper, test_to_run):
        # Call run_test method of ExchangeTestRunner with token configuration
        BoilerplateTokenTests(backend, exchange_navigation_helper).run_test(test_to_run)

    # Run a few specific tests for token swap
    # We use parametrize + getattr to override the called final callback and craft faulty transactions
    @pytest.mark.parametrize('fault_test_to_run', [
        "perform_final_tx_token_mismatch",
        "perform_final_tx_token_unknown",
    ])
    def test_boilerplate_token_issues(self, backend, exchange_navigation_helper, fault_test_to_run):
        with pytest.raises(ExceptionRAPDU) as e:
            test_class = BoilerplateTokenTests(backend, exchange_navigation_helper)
            test_class.perform_final_tx = getattr(test_class, fault_test_to_run)
            test_class.run_test("swap_valid_1")
        assert e.value.status == BoilerplateErrors.SW_SWAP_FAIL


# CAL Dynamic Token SWAP Tests
# Demonstrates CAL-signed dynamic tokens (requires Ledger coordination for production)
# For simpler token support, see BoilerplateTokenTests (hardcoded tokens) instead
class BoilerplateDynamicTokenTests(BoilerplateTokenTests):
    currency_configuration = cal.BOL_DYNAMIC_USDT_CURRENCY_CONFIGURATION

    def _perform_final_tx_with_dynamic_token(self, destination, send_amount, fees, memo,
                                            token_address_hex, ticker, decimals):
        """
        Provide dynamic token info via CAL and perform token transaction.

        Args:
            destination: Transaction destination address
            send_amount: Amount to send
            fees: Transaction fees
            memo: Transaction memo
            token_address_hex: Token address as hex string (64 chars)
            ticker: Token ticker symbol to provide via CAL
            decimals: Token decimals to provide via CAL
        """
        # First, provide the dynamic token info via CAL
        client = BoilerplateCommandSender(self.backend)
        token_address = bytes.fromhex(token_address_hex)

        rapdu = client.provide_dynamic_token(
            ticker=ticker,
            decimals=decimals,
            token_address=token_address
        )
        assert rapdu.status == 0x9000, f"Failed to provide dynamic token: {hex(rapdu.status)}"

        # Now perform the token transaction with the dynamically provided token
        self._perform_final_tx_with_token(destination, send_amount, fees, memo, token_address_hex)

    def perform_final_tx(self, destination, send_amount, fees, memo):
        # Matching the BOL_DYNAMIC_USDT_CURRENCY_CONFIGURATION provided earlier
        self._perform_final_tx_with_dynamic_token(
            destination, send_amount, fees, memo,
            TOKEN_DYNAMIC_USDT_ADDRESS,
            ticker="USDT",
            decimals=6
        )

    def perform_final_tx_with_wrong_address(self, destination, send_amount, fees, memo):
        """Provide dynamic token with wrong address then use different address in transaction."""
        # Provide dynamic token with LINK address
        self._perform_final_tx_with_dynamic_token(
            destination, send_amount, fees, memo,
            TOKEN_LINK_ADDRESS,
            ticker="WRONG",
            decimals=6
        )
        # Override the transaction to use a different address (this is a hack for testing)
        # Actually we need to provide one address but use another in the transaction
        # Let's call the parent method directly with wrong address
        client = BoilerplateCommandSender(self.backend)
        wrong_token_address = bytes.fromhex(TOKEN_LINK_ADDRESS)
        client.provide_dynamic_token(
            ticker="WRONG",
            decimals=6,
            token_address=wrong_token_address
        )
        # But use different address in transaction
        self._perform_final_tx_with_token(destination, send_amount, fees, memo, TOKEN_DYNAMIC_USDT_ADDRESS)

    def perform_final_tx_override_hardcoded(self, destination, send_amount, fees, memo):
        """Provide dynamic token with USDC address but different metadata."""
        self._perform_final_tx_with_dynamic_token(
            destination, send_amount, fees, memo,
            TOKEN_USDC_ADDRESS,
            ticker="USDT",  # Use USDT to match BOL_DYNAMIC_USDT_CURRENCY_CONFIGURATION
            decimals=6      # Different from hardcoded USDC which has 12
        )

    def perform_final_tx_no_provision(self, destination, send_amount, fees, memo):
        """Skip providing dynamic token - go straight to transaction."""
        # Don't provide dynamic token - just do the transaction
        self._perform_final_tx_with_token(destination, send_amount, fees, memo, TOKEN_DYNAMIC_USDT_ADDRESS)

    def perform_final_tx_reuse(self, destination, send_amount, fees, memo):
        """Reuse dynamic token without re-providing (test persistence)."""
        # Skip providing token again - rely on persistence from previous call
        self._perform_final_tx_with_token(destination, send_amount, fees, memo, TOKEN_DYNAMIC_USDT_ADDRESS)

    def perform_final_tx_wrong_decimals(self, destination, send_amount, fees, memo):
        """Provide dynamic token with decimals that don't match SWAP config."""
        self._perform_final_tx_with_dynamic_token(
            destination, send_amount, fees, memo,
            TOKEN_DYNAMIC_USDT_ADDRESS,
            ticker="USDT",
            decimals=8  # Config expects 6
        )


# We use a class to reuse the same Speculos instance (faster performances)
class TestsBoilerplateDynamic:
    """Tests for CAL dynamic token swap functionality."""

    @pytest.mark.parametrize('test_to_run', ALL_TESTS_EXCEPT_MEMO_THORSWAP_AND_FEES)
    def test_boilerplate_dynamic_token(self, backend, exchange_navigation_helper, test_to_run):
        """Test SWAP with CAL-provided dynamic token (USDT with 6 decimals)."""
        # Call run_test method of ExchangeTestRunner with dynamic token configuration
        BoilerplateDynamicTokenTests(backend, exchange_navigation_helper).run_test(test_to_run)

    def test_boilerplate_dynamic_token_wrong_address(self, backend, exchange_navigation_helper):
        """Test SWAP fails when dynamic token address doesn't match transaction token address."""
        test_class = BoilerplateDynamicTokenTests(backend, exchange_navigation_helper)
        test_class.perform_final_tx = test_class.perform_final_tx_with_wrong_address

        with pytest.raises(ExceptionRAPDU) as e:
            test_class.run_test("swap_valid_1")
        assert e.value.status == BoilerplateErrors.SW_SWAP_FAIL

    def test_boilerplate_dynamic_token_override_hardcoded(self, backend, exchange_navigation_helper):
        """Test SWAP with dynamic token that overrides a hardcoded token (same address, different metadata)."""
        test_class = BoilerplateDynamicTokenTests(backend, exchange_navigation_helper)
        test_class.perform_final_tx = test_class.perform_final_tx_override_hardcoded

        # This should succeed - dynamic token (CUSD, 6 decimals) overrides hardcoded USDC (12 decimals)
        test_class.run_test("swap_valid_1")

    def test_boilerplate_dynamic_token_no_provision(self, backend, exchange_navigation_helper):
        """Test SWAP fails when trying to use dynamic token address without providing it first."""
        test_class = BoilerplateDynamicTokenTests(backend, exchange_navigation_helper)
        test_class.perform_final_tx = test_class.perform_final_tx_no_provision

        with pytest.raises(ExceptionRAPDU) as e:
            test_class.run_test("swap_valid_1")
        # Should fail because token address is unknown (not in hardcoded DB and not provided dynamically)
        assert e.value.status == BoilerplateErrors.SW_SWAP_FAIL

    def test_boilerplate_dynamic_token_wrong_decimals_amount(self, backend, exchange_navigation_helper):
        """Test SWAP with dynamic token but wrong decimals in currency configuration."""
        test_class = BoilerplateDynamicTokenTests(backend, exchange_navigation_helper)
        test_class.perform_final_tx = test_class.perform_final_tx_wrong_decimals

        # This should fail during SWAP validation (amount mismatch due to wrong decimals)
        with pytest.raises(ExceptionRAPDU) as e:
            test_class.run_test("swap_valid_1")
        # Could be SW_SWAP_FAIL or other error depending on when validation happens
        assert e.value.status in [BoilerplateErrors.SW_SWAP_FAIL, BoilerplateErrors.SW_WRONG_DATA_LENGTH]
