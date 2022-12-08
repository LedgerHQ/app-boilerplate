from pathlib import Path
from typing import List, Dict, Any, Tuple
import unittest
import re

from boilerplate_client.errors import ERRORS


SW_RE = re.compile(r"""(?x)
    \#                                 # character '#'
    define                             # string 'define'
    \s+                                # spaces
    (?P<identifier>SW(?:_[A-Z0-9]+)*)  # identifier (e.g. 'SW_OK')
    \s+                                # spaces
    0x(?P<sw>[a-fA-F0-9]{4})           # 4 bytes status word
""")


def parse_sw(path: Path) -> Dict[str, int]:
    if not path.is_file():
        raise FileNotFoundError(f"Can't find file: '{path}'")

    sw_h: str = path.read_text()

    return {identifier: int(sw, base=16) for identifier, sw in SW_RE.findall(sw_h) if sw != "9000"}


def get_sw_h_path():
    # path with tests
    conftest_folder_path: Path = Path(__file__).parent
    # sw.h should be in ../../src/sw.h
    sw_h_path = conftest_folder_path.parent / "src" / "sw.h"
    if not sw_h_path.is_file():
        raise FileNotFoundError(f"Can't find sw.h: '{sw_h_path}'")
    return sw_h_path


def test_status_word():
    # Find the sw.h file containing the error apdu
    sw_h_file = get_sw_h_path()
    # Parse it to construct a dict of errors
    expected_status_words: List[Dict[str, int]] = parse_sw(sw_h_file)
    # Verify that this dict is the same as the one stored in python tests
    unittest.TestCase().assertCountEqual(expected_status_words, ERRORS)
