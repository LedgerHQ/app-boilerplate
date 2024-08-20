import pytest

from application_client.boilerplate_command_sender import BoilerplateCommandSender, Errors
from ragger.error import ExceptionRAPDU


# In this test we check that the TEST_REVIEW2 replies OK if the user accepts
def test_review2_accepted(backend, scenario_navigator):
    client = BoilerplateCommandSender(backend)
    with client.test_review2():
        scenario_navigator.address_review_approve()

    status = client.get_async_response().status

    assert status == 0x9000


# In this test we check that the TEST_REVIEW2 replies an error if the user refuses
def test_review2_refused(backend, scenario_navigator):
    client = BoilerplateCommandSender(backend)

    with pytest.raises(ExceptionRAPDU) as e:
        with client.test_review2():
            scenario_navigator.address_review_reject()

    # Assert that we have received a refusal
    assert e.value.status == Errors.SW_DENY
    assert len(e.value.data) == 0
