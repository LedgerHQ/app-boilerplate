from typing import Union


def der_encode(value: int) -> bytes:
    """
    Encode an integer using DER encoding for TLV tags and lengths.

    For values < 128, returns single byte.
    For values >= 128, uses long form: first byte is 0x80|length, followed by value bytes.

    Args:
        value: Integer to encode

    Returns:
        DER-encoded bytes
    """
    # max() to have minimum length of 1
    value_bytes = value.to_bytes(max(1, (value.bit_length() + 7) // 8), "big")
    if value >= 0x80:
        value_bytes = (0x80 | len(value_bytes)).to_bytes(1, "big") + value_bytes
    return value_bytes


def format_tlv(tag: int, value: Union[int, str, bytes]) -> bytes:
    """
    Format a TLV (Tag-Length-Value) entry for CAL dynamic token descriptor.

    Converts various Python types to properly formatted TLV bytes:
    - int: Encoded as big-endian bytes (minimum 1 byte)
    - str: Encoded as UTF-8 bytes
    - bytes: Used directly

    Args:
        tag: TLV tag number (e.g., 0x01 for STRUCTURE_TYPE)
        value: Value to encode (int, str, or bytes)

    Returns:
        Complete TLV entry: tag + length + value (all DER-encoded)

    Example:
        >>> format_tlv(0x01, 0x90)  # STRUCTURE_TYPE = DYNAMIC_TOKEN
        b'\\x01\\x01\\x90'
        >>> format_tlv(0x05, "USDC")  # TICKER
        b'\\x05\\x04USDC'
    """
    if isinstance(value, int):
        # max() to have minimum length of 1
        value = value.to_bytes(max(1, (value.bit_length() + 7) // 8), "big")
    elif isinstance(value, str):
        value = value.encode()

    assert isinstance(value, bytes), f"Unhandled TLV formatting for type: {type(value)}"

    tlv = b""
    tlv += der_encode(tag)
    tlv += der_encode(len(value))
    tlv += value
    return tlv
