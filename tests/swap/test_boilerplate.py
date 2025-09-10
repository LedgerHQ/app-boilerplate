import pytest
from ledger_app_clients.exchange.test_runner import ExchangeTestRunner, ALL_TESTS_EXCEPT_MEMO_THORSWAP_AND_FEES

from application_client.boilerplate_currency_utils import BOL_PATH
from application_client.boilerplate_command_sender import BoilerplateCommandSender, Errors as BoilerplateErrors
from application_client.boilerplate_transaction import Transaction

from . import cal_helper as cal

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
    wrong_amount_error_code = BoilerplateErrors.SW_WRONG_AMOUNT
    wrong_destination_error_code = BoilerplateErrors.SW_WRONG_ADDRESS

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


# We use a class to reuse the same Speculos instance (faster performances)
class TestsBoilerplate:
    # Run all the tests applicable to our setup: here we don't test fees mismatch, memo mismatch, and Thorswap / LiFi
    @pytest.mark.parametrize('test_to_run', ALL_TESTS_EXCEPT_MEMO_THORSWAP_AND_FEES)
    def test_boilerplate(self, backend, exchange_navigation_helper, test_to_run):
        # Call run_test method of ExchangeTestRunner
        BoilerplateTests(backend, exchange_navigation_helper).run_test(test_to_run)
