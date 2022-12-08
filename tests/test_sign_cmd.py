from hashlib import sha256
from sha3 import keccak_256
from pathlib import Path

from ecdsa.curves import SECP256k1
from ecdsa.keys import VerifyingKey
from ecdsa.util import sigdecode_der

from boilerplate_client.transaction import Transaction
from boilerplate_client.boilerplate_cmd import BoilerplateCommand
from ragger.navigator import NavInsID, NavIns
from utils import create_simple_nav_instructions

ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()

def test_sign_tx(backend, firmware, navigator, test_name):
    client = BoilerplateCommand(backend)
    bip32_path: str = "m/44'/0'/0'/0/0"

    pub_key, chain_code = client.get_public_key(
        bip32_path=bip32_path,
        display=False
    )  # type: bytes, bytes

    pk: VerifyingKey = VerifyingKey.from_string(
        pub_key,
        curve=SECP256k1,
        hashfunc=sha256
    )

    tx = Transaction(
        nonce=1,
        to="0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
        value=666,
        memo="For u EthDev"
    )

    with client.sign_tx(bip32_path=bip32_path, transaction=tx):
        if backend.firmware.device == "nanos":
            nav_ins = create_simple_nav_instructions(5)
        elif backend.firmware.device.startswith("nano"):
            nav_ins = create_simple_nav_instructions(3)
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins, first_instruction_wait=1.5, middle_instruction_wait=0.5, last_instruction_wait=1.0)

    response = backend.last_async_response.data
    # response = der_sig_len (1) ||
    #            der_sig (var) ||
    #            v (1)

    der_sig_len: int = response[0]
    der_sig: bytes = response[1:1 + der_sig_len]
    v: int = response[-1]
    assert len(response) == 1 + der_sig_len + 1
    assert pk.verify(signature=der_sig,
                     data=tx.serialize(),
                     hashfunc=keccak_256,
                     sigdecode=sigdecode_der)
