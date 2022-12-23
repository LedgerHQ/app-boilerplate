from typing import List
from pathlib import Path
from hashlib import sha256
from sha3 import keccak_256

from ecdsa.curves import SECP256k1
from ecdsa.keys import VerifyingKey
from ecdsa.util import sigdecode_der

from ragger.navigator import NavInsID, NavIns

ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()

# Helper to create a list of n right_clicks instructions and a final double button press
# useful for navigating through Nano menus
def create_simple_nav_instructions(right_clicks: int) -> List[NavIns]:
    instructions = [NavIns(NavInsID.RIGHT_CLICK)] * right_clicks
    instructions.append(NavIns(NavInsID.BOTH_CLICK))
    return instructions

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
