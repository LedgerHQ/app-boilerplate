import pytest
from ledger_app_clients.exchange.test_runner import ExchangeTestRunner, ALL_TESTS_EXCEPT_MEMO_THORSWAP_AND_FEES
from .apps import cal as cal

from .apps.boilerplate import BOL_PATH
from .apps.boilerplate_application_client.boilerplate_command_sender import BoilerplateCommandSender, Errors as BoilerplateErrors
from .apps.boilerplate_application_client.boilerplate_transaction import Transaction 

# ExchangeTestRunner implementation for Near
class BoilerplateTests(ExchangeTestRunner):

    currency_configuration = cal.BOL_CURRENCY_CONFIGURATION
    valid_destination_1 = "de0b295669a9fd93d5f28d9ec85e40f4cb697bae"
    valid_destination_memo_1 = ""
    valid_destination_2 = "9fc3da866e7dF3a1c57ade1a97c9f00a70f010c8"
    valid_destination_memo_2 = ""
    valid_refund = "FD2095A37E72BE2CD575D18FE8F16E78C51EAFA3"
    valid_refund_memo = ""
    valid_send_amount_1 = 1000
    valid_send_amount_2 = 666
    valid_fees_1 = 0
    valid_fees_2 = 0
    fake_refund = "abcdabcd"
    fake_refund_memo = "bla"
    fake_payout = "abcdabcd"
    fake_payout_memo = "bla"
    signature_refusal_error_code = BoilerplateErrors.SW_DENY
    wrong_amount_error_code = BoilerplateErrors.SW_WRONG_AMOUNT
    wrong_destination_error_code = BoilerplateErrors.SW_WRONG_ADDRESS

    def perform_final_tx(self, destination, send_amount, fees, memo):
        # Create the transaction that will be sent to the device for signing
        tx = Transaction(
            nonce=1,
            to=destination,
            value=send_amount,
            memo=memo
        ).serialize()

        BoilerplateCommandSender(self.backend).sign_tx(path=BOL_PATH, transaction=tx)

        # TODO : assert signature validity


# Use a class to reuse the same Speculos instance
class TestsBoilerplate:

    @pytest.mark.parametrize('test_to_run', ALL_TESTS_EXCEPT_MEMO_THORSWAP_AND_FEES)
    def test_boilerplate(self, backend, exchange_navigation_helper, test_to_run):
        BoilerplateTests(backend, exchange_navigation_helper).run_test(test_to_run)