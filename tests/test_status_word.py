from typing import List, Tuple, Dict, Any

import pytest

from boilerplate_client.exception import DeviceException
from boilerplate_client.utils import parse_sw


@pytest.mark.checksw
def test_status_word(sw_h_path):
    expected_status_words: List[Tuple[str, int]] = parse_sw(sw_h_path)
    status_words: Dict[int, Any] = DeviceException.exc

    assert (len(expected_status_words) == len(status_words),
            f"{expected_status_words} doesn't match {status_words}")

    # just keep status words
    expected_status_words = [sw for (identifier, sw) in expected_status_words]

    for sw in status_words.keys():
        assert sw in expected_status_words, f"{status_words[sw]}({hex(sw)}) not found in sw.h!"
