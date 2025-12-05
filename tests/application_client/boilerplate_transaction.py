from io import BytesIO
from typing import Union

from .boilerplate_utils import (
    read,
    read_uint,
    read_varint,
    write_varint,
    UINT64_MAX,
    parse_hex_address,
)


class TransactionError(Exception):
    pass


class Transaction:
    def __init__(
        self, nonce: int, to: Union[str, bytes], value: int, memo: str
    ) -> None:
        self.nonce: int = nonce
        self.to: bytes = parse_hex_address(to)
        self.value: int = value
        self.memo: bytes = memo.encode("ascii")

        if not 0 <= self.nonce <= UINT64_MAX:
            raise TransactionError(f"Bad nonce: '{self.nonce}'!")

        if not 0 <= self.value <= UINT64_MAX:
            raise TransactionError(f"Bad value: '{self.value}'!")

        if len(self.to) != 20:
            raise TransactionError(f"Bad address: '{self.to.hex()}'!")

    def serialize(self) -> bytes:
        return b"".join(
            [
                self.nonce.to_bytes(8, byteorder="big"),
                self.to,
                self.value.to_bytes(8, byteorder="big"),
                write_varint(len(self.memo)),
                self.memo,
            ]
        )

    @classmethod
    def from_bytes(cls, hexa: Union[bytes, BytesIO]):
        buf: BytesIO = BytesIO(hexa) if isinstance(hexa, bytes) else hexa

        nonce: int = read_uint(buf, 64, byteorder="big")
        to: bytes = read(buf, 20)
        value: int = read_uint(buf, 64, byteorder="big")
        memo_len: int = read_varint(buf)
        memo: str = read(buf, memo_len).decode("ascii")

        return cls(nonce=nonce, to=to, value=value, memo=memo)


class TokenTransaction:
    def __init__(
        self,
        nonce: int,
        to: Union[str, bytes],
        token_address: Union[str, bytes],
        value: int,
        memo: str,
    ) -> None:
        self.nonce: int = nonce
        self.to: bytes = parse_hex_address(to)
        self.token_address: bytes = parse_hex_address(token_address)
        self.value: int = value
        self.memo: bytes = memo.encode("ascii")

        if not 0 <= self.nonce <= UINT64_MAX:
            raise TransactionError(f"Bad nonce: '{self.nonce}'!")

        if not 0 <= self.value <= UINT64_MAX:
            raise TransactionError(f"Bad value: '{self.value}'!")

        if len(self.to) != 20:
            raise TransactionError(f"Bad address: '{self.to.hex()}'!")

        if len(self.token_address) != 32:
            raise TransactionError(f"Bad token address: '{self.token_address.hex()}'!")

    def serialize(self) -> bytes:
        return b"".join(
            [
                self.nonce.to_bytes(8, byteorder="big"),
                self.to,
                self.token_address,
                self.value.to_bytes(8, byteorder="big"),
                write_varint(len(self.memo)),
                self.memo,
            ]
        )

    @classmethod
    def from_bytes(cls, hexa: Union[bytes, BytesIO]):
        buf: BytesIO = BytesIO(hexa) if isinstance(hexa, bytes) else hexa

        nonce: int = read_uint(buf, 64, byteorder="big")
        to: bytes = read(buf, 20)
        token_address: bytes = read(buf, 32)
        value: int = read_uint(buf, 64, byteorder="big")
        memo_len: int = read_varint(buf)
        memo: str = read(buf, memo_len).decode("ascii")

        return cls(
            nonce=nonce, to=to, token_address=token_address, value=value, memo=memo
        )
