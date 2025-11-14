from typing import Tuple
from struct import unpack

# remainder, data_len, data
def pop_sized_buf_from_buffer(buffer:bytes, size:int) -> Tuple[bytes, bytes]:
    return buffer[size:], buffer[0:size]

# remainder, data_len, data
def pop_size_prefixed_buf_from_buf(buffer:bytes) -> Tuple[bytes, int, bytes]:
    data_len = buffer[0]
    return buffer[1+data_len:], data_len, buffer[1:data_len+1]

# Unpack from response:
# response = app_name (var)
def unpack_get_app_name_response(response: bytes) -> str:
    return response.decode("ascii")

# Unpack from response:
# response = MAJOR (1)
#            MINOR (1)
#            PATCH (1)
def unpack_get_version_response(response: bytes) -> Tuple[int, int, int]:
    assert len(response) == 3
    major, minor, patch = unpack("BBB", response)
    return (major, minor, patch)

# Unpack from response:
# response = serial (7)
def unpack_get_serial_response(response: bytes) -> bytes:
    SERIAL_LENGTH = 7
    assert len(response) == SERIAL_LENGTH
    return response

# Unpack from response:
# response = format_id (1)
#            app_name_raw_len (1)
#            app_name_raw (var)
#            version_raw_len (1)
#            version_raw (var)
#            unused_len (1)
#            unused (var)
def unpack_get_app_and_version_response(response: bytes) -> Tuple[str, str]:
    response, _ = pop_sized_buf_from_buffer(response, 1)
    response, _, app_name_raw = pop_size_prefixed_buf_from_buf(response)
    response, _, version_raw = pop_size_prefixed_buf_from_buf(response)
    response, _, _ = pop_size_prefixed_buf_from_buf(response)

    assert len(response) == 0

    return app_name_raw.decode("ascii"), version_raw.decode("ascii")

# Unpack from response:
# response =
#            pub_key (var)
#            chain_code (var)
def unpack_get_public_key_response(response: bytes) -> Tuple[bytes, bytes]:
    PUBLIC_KEY_SIZE = 32
    CHAIN_CODE_SIZE = 32
    assert len(response) == PUBLIC_KEY_SIZE + CHAIN_CODE_SIZE
    public_key = response[:PUBLIC_KEY_SIZE]
    chain_code = response[PUBLIC_KEY_SIZE:]

    return public_key, chain_code

# Unpack from response:
# response = der_sig_len (1)
#            der_sig (var)
#            v (1)
def unpack_sign_tx_response(response: bytes) -> Tuple[int, bytes, int]:
    response, der_sig_len, der_sig = pop_size_prefixed_buf_from_buf(response)
    response, v = pop_sized_buf_from_buffer(response, 1)

    assert len(response) == 0

    return der_sig_len, der_sig, int.from_bytes(v, byteorder='big')
