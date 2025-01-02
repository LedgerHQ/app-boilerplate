from pathlib import Path
from typing import List
import re
from hashlib import sha256
from sha3 import keccak_256

from ecdsa.curves import SECP256k1
from ecdsa.keys import VerifyingKey
from ecdsa.util import sigdecode_der


# Check if a signature of a given message is valid
def check_signature_validity(public_key: bytes, signature: bytes, message: bytes) -> bool:
    pk: VerifyingKey = VerifyingKey.from_string(
        public_key,
        curve=SECP256k1,
        hashfunc=sha256
    )
    return pk.verify(signature=signature,
                     data=message,
                     hashfunc=keccak_256,
                     sigdecode=sigdecode_der)


def verify_name(name: str) -> None:
    """Verify the app name, based on defines in Makefile

    Args:
        name (str): Name to be checked
    """

    name_str = ""
    lines = _read_makefile()
    name_re = re.compile(r"^APPNAME\s?=\s?\"?(?P<val>\w+)\"?", re.I)
    for line in lines:
        info = name_re.match(line)
        if info:
            dinfo = info.groupdict()
            name_str = dinfo["val"]
    assert name == name_str


def verify_version(version: str) -> None:
    """Verify the app version, based on defines in Makefile

    Args:
        Version (str): Version to be checked
    """

    vers_dict = {}
    vers_str = ""
    lines = _read_makefile()
    version_re = re.compile(r"^APPVERSION_(?P<part>\w)\s?=\s?(?P<val>\d*)", re.I)
    for line in lines:
        info = version_re.match(line)
        if info:
            dinfo = info.groupdict()
            vers_dict[dinfo["part"]] = dinfo["val"]
    try:
        vers_str = f"{vers_dict['M']}.{vers_dict['N']}.{vers_dict['P']}"
    except KeyError:
        pass
    assert version == vers_str


def _read_makefile() -> List[str]:
    """Read lines from the parent Makefile """

    parent = Path(__file__).parent.parent.resolve()
    makefile = f"{parent}/Makefile"
    with open(makefile, "r", encoding="utf-8") as f_p:
        lines = f_p.readlines()
    return lines
