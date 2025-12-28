from pathlib import Path
from typing import List, Tuple, Union
import re
import hashlib
from Crypto.Hash import keccak

from ecdsa.curves import SECP256k1, Ed25519
from ecdsa.keys import VerifyingKey
from ecdsa.util import sigdecode_der

from bip_utils import Bip44, Bip44Coins, Bip44Changes, Bip39SeedGenerator
from bip_utils.bip.bip32.bip32_path import Bip32Path, Bip32PathParser

from ragger.bip import calculate_public_key_and_chaincode, CurveChoice
from ragger.conftest.configuration import OPTIONAL

from ragger.navigator import NavInsID, NavIns, Navigator
from ragger.bip.seed import SPECULOS_MNEMONIC

from ledgered.devices import DeviceType, Device

from application_client.app_def import AddressType

from standalone.input_files.derive_address import DeriveAddressTestCase
from standalone.input_files.pubkey import PubKeyTestCase
from standalone.input_files.cvote import CVoteTestCase
from standalone.input_files.signOpCert import OpCertTestCase
from standalone.input_files.signMsg import SignMsgTestCase


ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()


# Check if a signature of a given message is valid
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

    parent = Path(__file__).parent.parent.parent.resolve()
    makefile = f"{parent}/Makefile"
    with open(makefile, "r", encoding="utf-8") as f_p:
        lines = f_p.readlines()
    return lines



def idTestFunc(testCase: Union[DeriveAddressTestCase, PubKeyTestCase, CVoteTestCase, OpCertTestCase, SignMsgTestCase]) -> str:
    """Retrieve the test case name for friendly display

    Args:
        testCase (xxxTestCase): Targeted test case

    Returns:
        Test case name
    """
    return testCase.name


def pop_sized_buf_from_buffer(buffer:bytes, size:int) -> Tuple[bytes, bytes]:
    """Extract a buffer of a given size from a buffer

    Args:
        buffer (bytes): Source buffer
        size (int): Size of the buffer to extract

    Returns:
        Tuple of:
            - The remaining buffer
            - The extracted buffer
    """
    return buffer[size:], buffer[0:size]


def pop_size_prefixed_buf_from_buf(buffer:bytes, lenSize:int) -> Tuple[bytes, int, bytes]:
    """Extract a buffer prefixed with its size from a buffer

    Args:
        buffer (bytes): Source buffer
        lenSize (int): Size of the length prefix

    Returns:
        Tuple of:
            - The remaining buffer
            - The extracted data length
            - The extracted buffer
    """
    data_len = int.from_bytes(buffer[0:lenSize], "big")
    return buffer[lenSize+data_len:], data_len, buffer[lenSize:data_len+lenSize]


def derive_address(testCase: DeriveAddressTestCase) -> Union[bytes, str]:
    """Derive an address from a test case

    Args:
        testCase (DeriveAddressTestCase): The test case

    Returns:
        The derived address
    """

    if testCase.addrType == AddressType.BYRON:
        return _deriveAddressByron(testCase)
    return _deriveAddressShelley(testCase)


def _deriveAddressByron(testCase: DeriveAddressTestCase) -> str:
    """Derive the Byron address from the path"""
   # Generate seed from mnemonic
   # seed_bytes = Bip39SeedGenerator(OPTIONAL.CUSTOM_SEED).Generate()
    seed_bytes = Bip39SeedGenerator(SPECULOS_MNEMONIC).Generate()

    # Construct from seed
    bip44_mst_ctx = Bip44.FromSeed(seed_bytes, Bip44Coins.CARDANO_BYRON_LEDGER)

    # Derive the key for the specified path
    bip32Path: Bip32Path = Bip32PathParser.Parse(testCase.spendingValue).ToList()
    bip44_acc = bip44_mst_ctx.Purpose().Coin().Account(bip32Path[2])
    bip44_chg = bip44_acc.Change(Bip44Changes.CHAIN_EXT if bip32Path[3] == 0 else Bip44Changes.CHAIN_INT)
    bip44_addr = bip44_chg.AddressIndex(bip32Path[4])

    # Get the address
    return bip44_addr.PublicKey().ToAddress()


def _deriveAddressShelley(testCase: DeriveAddressTestCase) -> bytes:
    """Derive the Shelley base address from the path"""
    key = f"{(int(testCase.addrType) << 4) | int(testCase.netDesc.networkId):02x}"
    if testCase.spendingValue.startswith("m/"):
        pk, _ = get_device_pubkey(testCase.spendingValue)
        key += hashlib.blake2b(pk, digest_size=28).digest().hex()
    else:
        key += testCase.spendingValue
    if testCase.addrType in (AddressType.POINTER_KEY,
                             AddressType.POINTER_SCRIPT):
        key += _appenduint32(int(testCase.stakingValue[0:8], 16))
        key += _appenduint32(int(testCase.stakingValue[8:16], 16))
        key += _appenduint32(int(testCase.stakingValue[16:24], 16))
    elif testCase.stakingValue.startswith("m/"):
        pk, _ = get_device_pubkey(testCase.stakingValue)
        key += hashlib.blake2b(pk, digest_size=28).digest().hex()
    else:
        key += testCase.stakingValue
    return bytes.fromhex(key)


def _appenduint32(value: int) -> str:
    """Append a Variable Length uint32 to a buffer"""

    if value == 0:
        return "00"

    chunks: list[int] = []
    while value:
        chunks.append(value & 0x7F)
        value >>= 7

    result = ""
    while len(chunks) > 1:
        result += f"{chunks.pop() | 0x80:02x}"
    result += f"{chunks.pop():02x}"
    return result


def get_device_pubkey(path: str) -> Tuple[bytes, str]:
    """ Retrieve the Public Key

    Args:
        path (str): Derivation path

    Returns:
        The Reference PK and the byte Chain Code
    """
    ref_pk, ref_chain_code = calculate_public_key_and_chaincode(CurveChoice.Ed25519Kholaw, path)
    return bytes.fromhex(ref_pk[2:]), ref_chain_code


def verify_signature(path: str, signature: bytes, data: bytes) -> None:
    """Check the signature validity

    Args:
        path (str): The derivation path
        signature (bytes): The received signature
        data (bytes): The signed data
    """

    ref_pk, _ = get_device_pubkey(path)
    pk: VerifyingKey = VerifyingKey.from_string(ref_pk, curve=Ed25519)
    assert pk.verify(signature, data, hashlib.sha512)
