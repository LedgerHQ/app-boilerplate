import struct
from typing import Tuple, Generator
from contextlib import contextmanager

from ragger.backend.interface import BackendInterface, RAPDU

from boilerplate_client.boilerplate_cmd_builder import BoilerplateCommandBuilder
from boilerplate_client.transaction import Transaction


class BoilerplateCommand:
    def __init__(self,
                 client: BackendInterface,
                 debug: bool = False) -> None:
        self.client = client
        self.builder = BoilerplateCommandBuilder(debug=debug)
        self.debug = debug

    def get_app_and_version(self) -> Tuple[str, str]:
        response = self.client.exchange_raw(
            self.builder.get_app_and_version()
        ).data

        offset: int = 1
        app_name_len: int = response[offset]
        offset += 1
        app_name: str = response[offset:offset + app_name_len].decode("ascii")
        offset += app_name_len
        version_len: int = response[offset]
        offset += 1
        version: str = response[offset:offset + version_len].decode("ascii")
        offset += version_len

        return app_name, version

    def get_version(self) -> Tuple[int, int, int]:
        response = self.client.exchange_raw(
            self.builder.get_version()
        ).data

        # response = MAJOR (1) || MINOR (1) || PATCH (1)
        assert len(response) == 3

        major, minor, patch = struct.unpack(
            "BBB",
            response
        )  # type: int, int, int

        return major, minor, patch

    def get_app_name(self) -> str:
        response = self.client.exchange_raw(
            self.builder.get_app_name()
        ).data

        return response.decode("ascii")

    def get_public_key(self, bip32_path: str, display: bool = False) -> Tuple[bytes, bytes]:
        response = self.client.exchange_raw(
            self.builder.get_public_key(bip32_path=bip32_path,
                                        display=display)
        ).data

        # response = pub_key_len (1) ||
        #            pub_key (var) ||
        #            chain_code_len (1) ||
        #            chain_code (var)
        offset: int = 0

        pub_key_len: int = response[offset]
        offset += 1
        pub_key: bytes = response[offset:offset + pub_key_len]
        offset += pub_key_len
        chain_code_len: int = response[offset]
        offset += 1
        chain_code: bytes = response[offset:offset + chain_code_len]
        offset += chain_code_len

        assert len(response) == 1 + pub_key_len + 1 + chain_code_len

        return pub_key, chain_code

    @contextmanager
    def sign_tx(self, bip32_path: str, transaction: Transaction) -> Generator[RAPDU, None, None]:
        for is_last, chunk in self.builder.sign_tx(bip32_path=bip32_path, transaction=transaction):
            if not is_last:
                self.client.exchange_raw(chunk)
            else:
                with self.client.exchange_async_raw(chunk) as response:
                    yield response
