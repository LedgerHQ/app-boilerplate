"""Record the APDUs and user actions of the tests tagged as documentation flows.

A test tagged with ``@pytest.mark.flow(name, variant)`` is a working example of one flow of
``doc/FLOWS.md``. When pytest runs with ``--flows_dir <dir>``, every tagged test writes its
exchanges to ``<dir>/<device>/<test name>.json``. ``generate_flows.py`` then renders these
recordings into ``doc/FLOWS.md``.

The flow is maintained by construction: as long as the test is green, the documented exchange is
the real one.

Marker arguments:
    name:    flow identifier, matching a ``<!-- BEGIN GENERATED FLOW <name> -->`` block
    variant: label of this test in the flow, e.g. "User approves" (default: "Nominal")
    start:   optional command name, the exchanges before its first occurrence are ignored
             (e.g. the GET_PUBLIC_KEY sent by the test to check the signature)
"""

import json
import logging
import re
from pathlib import Path

import pytest
from ragger.logger import get_apdu_logger
from ragger.navigator.navigation_scenario import NavigateWithScenario

# User actions recorded from the scenario navigator: method name -> (screen, action)
USER_ACTIONS = {
    "review_approve": ("Review", "Approve"),
    "review_approve_with_warning": ("Review with warning", "Approve"),
    "review_approve_with_spinner": ("Review", "Approve"),
    "review_reject": ("Review", "Reject"),
    "review_reject_with_warning": ("Review with warning", "Reject"),
    "address_review_approve": ("Address", "Approve"),
    "address_review_reject": ("Address", "Reject"),
}

_APDU_LINE = re.compile(r"^(=>|<=) ?([0-9a-fA-F ]*)$")


class _FlowRecorder(logging.Handler):
    """Collect APDUs from the ragger APDU logger and user actions from the navigator."""

    def __init__(self) -> None:
        super().__init__(level=logging.INFO)
        self.steps: list[dict] = []

    def emit(self, record: logging.LogRecord) -> None:
        match = _APDU_LINE.match(record.getMessage().strip())
        if match is None:
            return
        direction, payload = match.group(1), match.group(2).replace(" ", "")
        if direction == "=>":
            self.steps.append({"type": "command", "apdu": payload})
        else:
            # Responses are logged as <data><status word>
            self.steps.append({"type": "response", "data": payload[:-4], "sw": payload[-4:]})

    def user(self, method: str, snapshots: str) -> None:
        screen, action = USER_ACTIONS[method]
        self.steps.append({"type": "user", "screen": screen, "action": action, "snapshots": snapshots})


# Recorder of the running flow test, if any
_STATE: dict[str, _FlowRecorder | None] = {"recorder": None}


def _wrap_navigator_method(method: str):
    original = getattr(NavigateWithScenario, method)

    def wrapper(self, *args, **kwargs):
        recorder = _STATE["recorder"]
        if recorder is not None:
            recorder.user(method, str(kwargs.get("test_name") or self.test_name))
        return original(self, *args, **kwargs)

    wrapper.__wrapped__ = original
    return wrapper


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--flows_dir",
        action="store",
        default=None,
        help="Record the tests tagged with @pytest.mark.flow into this directory",
    )


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line(
        "markers",
        "flow(name, variant='Nominal', start=None): the test is a working example of a doc/FLOWS.md flow",
    )
    for method in USER_ACTIONS:
        if hasattr(NavigateWithScenario, method):
            setattr(NavigateWithScenario, method, _wrap_navigator_method(method))


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Item, call: pytest.CallInfo):
    outcome = yield
    report = outcome.get_result()
    if report.when == "call":
        item.flow_test_passed = report.passed


@pytest.fixture(autouse=True)
def _record_flow(request: pytest.FixtureRequest):
    marker = request.node.get_closest_marker("flow")
    flows_dir = request.config.getoption("flows_dir")
    if marker is None or flows_dir is None:
        yield
        return

    recorder = _FlowRecorder()
    apdu_logger = get_apdu_logger()
    previous_level = apdu_logger.level
    apdu_logger.setLevel(logging.INFO)
    apdu_logger.addHandler(recorder)
    _STATE["recorder"] = recorder
    try:
        yield
    finally:
        _STATE["recorder"] = None
        apdu_logger.removeHandler(recorder)
        apdu_logger.setLevel(previous_level)

    # Only a green test is a valid example of the flow
    if not getattr(request.node, "flow_test_passed", False):
        return

    device = request.config.getoption("device")
    name = marker.kwargs.get("name", marker.args[0] if marker.args else None)
    out = Path(flows_dir) / device / f"{request.node.name}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        json.dumps(
            {
                "flow": name,
                "variant": marker.kwargs.get("variant", "Nominal"),
                "start": marker.kwargs.get("start"),
                "test": request.node.nodeid,
                # Variants of a flow are documented in the order of the tests in their file
                "location": [request.node.location[0], request.node.location[1]],
                "device": device,
                "steps": recorder.steps,
            },
            indent=2,
        )
        + "\n"
    )
