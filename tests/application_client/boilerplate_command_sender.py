from enum import IntEnum
from typing import Generator, List, Optional
from contextlib import contextmanager

from ragger.backend.interface import BackendInterface, RAPDU
from ragger.bip import pack_derivation_path

from .tlv import format_tlv
from .boilerplate_keychain import sign_data, Key
from .pki_client import (
    PKIClient,
    PKI_CERTIFICATES,
    CertificatePubKeyUsage
)


MAX_APDU_LEN: int = 255

CLA: int = 0xE0

class P1(IntEnum):
    # Parameter 1 for first APDU number.
    P1_START = 0x00
    # Parameter 1 for maximum APDU number.
    P1_MAX   = 0x03
    # Parameter 1 for screen confirmation for GET_PUBLIC_KEY.
    P1_CONFIRM = 0x01

class P2(IntEnum):
    # Parameter 2 for last APDU to receive.
    P2_LAST = 0x00
    # Parameter 2 for more APDU to receive.
    P2_MORE = 0x80

class InsType(IntEnum):
    GET_VERSION        = 0x03
    GET_APP_NAME       = 0x04
    GET_PUBLIC_KEY     = 0x05
    SIGN_TX            = 0x06
    SIGN_TOKEN_TX      = 0x07
    PROVIDE_TOKEN_INFO = 0x22

class Errors(IntEnum):
    SW_DENY                    = 0x6985
    SW_WRONG_P1P2              = 0x6A86
    SW_WRONG_DATA_LENGTH       = 0x6A87
    SW_INS_NOT_SUPPORTED       = 0x6D00
    SW_CLA_NOT_SUPPORTED       = 0x6E00
    SW_WRONG_RESPONSE_LENGTH   = 0xB000
    SW_DISPLAY_BIP32_PATH_FAIL = 0xB001
    SW_DISPLAY_ADDRESS_FAIL    = 0xB002
    SW_DISPLAY_AMOUNT_FAIL     = 0xB003
    SW_WRONG_TX_LENGTH         = 0xB004
    SW_TX_PARSING_FAIL         = 0xB005
    SW_TX_HASH_FAIL            = 0xB006
    SW_BAD_STATE               = 0xB007
    SW_SIGNATURE_FAIL          = 0xB008
    SW_INVALID_DYNAMIC_TOKEN   = 0xB009
    SW_SWAP_FAIL               = 0xC000


def split_message(message: bytes, max_size: int) -> List[bytes]:
    return [message[x:x + max_size] for x in range(0, len(message), max_size)]


class BoilerplateCommandSender:
    def __init__(self, backend: BackendInterface) -> None:
        self.backend = backend

    def get_app_and_version(self) -> RAPDU:
        return self.backend.exchange(cla=0xB0,  # specific CLA for BOLOS
                                     ins=0x01,  # specific INS for get_app_and_version
                                     p1=P1.P1_START,
                                     p2=P2.P2_LAST,
                                     data=b"")


    def get_version(self) -> RAPDU:
        return self.backend.exchange(cla=CLA,
                                     ins=InsType.GET_VERSION,
                                     p1=P1.P1_START,
                                     p2=P2.P2_LAST,
                                     data=b"")


    def get_app_name(self) -> RAPDU:
        return self.backend.exchange(cla=CLA,
                                     ins=InsType.GET_APP_NAME,
                                     p1=P1.P1_START,
                                     p2=P2.P2_LAST,
                                     data=b"")


    def get_public_key(self, path: str) -> RAPDU:
        return self.backend.exchange(cla=CLA,
                                     ins=InsType.GET_PUBLIC_KEY,
                                     p1=P1.P1_START,
                                     p2=P2.P2_LAST,
                                     data=pack_derivation_path(path))


    @contextmanager
    def get_public_key_with_confirmation(self, path: str) -> Generator[None, None, None]:
        with self.backend.exchange_async(cla=CLA,
                                         ins=InsType.GET_PUBLIC_KEY,
                                         p1=P1.P1_CONFIRM,
                                         p2=P2.P2_LAST,
                                         data=pack_derivation_path(path)) as response:
            yield response


    @contextmanager
    def _sign_transaction(self, ins: InsType, path: str, transaction: bytes) -> Generator[None, None, None]:
        """Generic transaction signing handler (for SIGN_TX and SIGN_TOKEN_TX)."""
        self.backend.exchange(cla=CLA,
                              ins=ins,
                              p1=P1.P1_START,
                              p2=P2.P2_MORE,
                              data=pack_derivation_path(path))
        messages = split_message(transaction, MAX_APDU_LEN)
        idx: int = P1.P1_START + 1

        for msg in messages[:-1]:
            self.backend.exchange(cla=CLA,
                                  ins=ins,
                                  p1=idx,
                                  p2=P2.P2_MORE,
                                  data=msg)
            idx += 1

        with self.backend.exchange_async(cla=CLA,
                                         ins=ins,
                                         p1=idx,
                                         p2=P2.P2_LAST,
                                         data=messages[-1]) as response:
            yield response

    # sign_tx and sign_token_tx are async functions that send the transaction to the device
    # Both functions yield in order to allow the caller to call the navigation functions
    @contextmanager
    def sign_tx(self, path: str, transaction: bytes) -> Generator[None, None, None]:
        with self._sign_transaction(InsType.SIGN_TX, path, transaction) as response:
            yield response

    @contextmanager
    def sign_token_tx(self, path: str, transaction: bytes) -> Generator[None, None, None]:
        with self._sign_transaction(InsType.SIGN_TOKEN_TX, path, transaction) as response:
            yield response

    # Retrieve the last asynchronous response from the backend
    def get_async_response(self) -> Optional[RAPDU]:
        return self.backend.last_async_response

    # Synchronous versions of sign_tx and sign_token_tx
    # These functions wait for the response after sending the transaction. They are particularly
    # useful for tests that do not require user interaction (e.g., when the transaction as already
    # been approved in the SWAP flow)
    def sign_tx_sync(self, path: str, transaction: bytes) -> Optional[RAPDU]:
        with self.sign_tx(path, transaction):
            pass
        rapdu = self.get_async_response()
        assert isinstance(rapdu, RAPDU)
        return rapdu

    def sign_token_tx_sync(self, path: str, transaction: bytes) -> Optional[RAPDU]:
        with self.sign_token_tx(path, transaction):
            pass
        rapdu = self.get_async_response()
        assert isinstance(rapdu, RAPDU)
        return rapdu

    def provide_dynamic_token(self,
                              ticker: str,
                              decimals: int,
                              token_address: bytes,
                              chain_id: int = 0x8001) -> RAPDU:
        """
        Provide a dynamic token descriptor signed with CAL test key.

        Args:
            ticker: Token ticker symbol (e.g., "USDC", "WETH")
            decimals: Number of decimal places (e.g., 6 for USDC, 18 for WETH)
            token_address: 32-byte token address for TUID
            chain_id: SLIP-44 coin type (default 0x8001 for boilerplate test coin)

        Returns:
            RAPDU response from the device
        """
        assert len(token_address) == 32, f"Token address must be 32 bytes, got {len(token_address)}"

        # Build TUID sub-TLV: tag 0x10 with 32-byte address (Boilerplate specific example)
        tuid = format_tlv(0x10, token_address)

        # Build payload WITHOUT signature for now
        payload = format_tlv(0x01, 0x90)  # STRUCTURE_TYPE: DYNAMIC_TOKEN
        payload += format_tlv(0x02, 0x01)  # VERSION: 1
        payload += format_tlv(0x03, chain_id)  # COIN_TYPE
        payload += format_tlv(0x04, "Boilerplate")  # APP: application name
        payload += format_tlv(0x05, ticker)  # TICKER
        payload += format_tlv(0x06, decimals)  # MAGNITUDE (decimals)
        payload += format_tlv(0x07, tuid)  # TUID

        # Sign the crafted payload and append the signature to it
        payload += format_tlv(0x08, sign_data(payload, key=Key.DYNAMIC_TOKEN))  # SIGNATURE

        # Before sending the mock CAL descriptor, we need to onboard our mock CAL using a PKI certificate
        cert_apdu = PKI_CERTIFICATES.get(self.backend.device.type)
        if cert_apdu:
            PKIClient(self.backend).send_certificate(
                CertificatePubKeyUsage.CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
                bytes.fromhex(cert_apdu)
                )

        # Send APDU with P1=0, P2=0 (no chunking for token descriptors)
        return self.backend.exchange(cla=CLA,
                                     ins=InsType.PROVIDE_TOKEN_INFO,
                                     p1=0x00,
                                     p2=0x00,
                                     data=payload)
