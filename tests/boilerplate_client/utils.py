from pathlib import Path
from typing import List, Tuple
import re


SW_RE = re.compile(r"""(?x)
    \#                                 # character '#'
    define                             # string 'define'
    \s+                                # spaces
    (?P<identifier>SW(?:_[A-Z0-9]+)*)  # identifier (e.g. 'SW_OK')
    \s+                                # spaces
    0x(?P<sw>[a-fA-F0-9]{4})           # 4 bytes status word
""")


def parse_sw(path: Path) -> List[Tuple[str, int]]:
    if not path.is_file():
        raise FileNotFoundError(f"Can't find file: '{path}'")

    sw_h: str = path.read_text()

    return [(identifier, int(sw, base=16))
            for identifier, sw in SW_RE.findall(sw_h) if sw != "9000"]


def bip32_path_from_string(path: str) -> List[bytes]:
    splitted_path: List[str] = path.split("/")

    if not splitted_path:
        raise Exception(f"BIP32 path format error: '{path}'")

    if "m" in splitted_path and splitted_path[0] == "m":
        splitted_path = splitted_path[1:]

    return [int(p).to_bytes(4, byteorder="big") if "'" not in p
            else (0x80000000 | int(p[:-1])).to_bytes(4, byteorder="big")
            for p in splitted_path]
