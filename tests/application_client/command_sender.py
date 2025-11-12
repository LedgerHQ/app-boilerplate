from enum import IntEnum
from typing import Generator, Optional
from contextlib import contextmanager

from ragger.backend.interface import BackendInterface, RAPDU

from standalone.input_files.signOpCert import OpCertTestCase
from application_client.command_builder import CommandBuilder, InsType


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
    SW_WRONG_AMOUNT            = 0xC000
    SW_WRONG_ADDRESS           = 0xC000
    SW_SUCCESS                 = 0x9000
    SW_REJECTED_BY_POLICY      = 0x6E10


class CommandSender:
    def __init__(self, backend: BackendInterface) -> None:
        self.backend = backend
        self._cmd_builder = CommandBuilder()

    def _exchange(self, payload: bytes) -> RAPDU:
        """Synchronous APDU exchange with response

        Args:
            payload (bytes): APDU data to send

        Returns:
            Response APDU
        """

        return self.backend.exchange_raw(payload)


    @contextmanager
    def _exchange_async(self, payload: bytes) -> Generator[None, None, None]:
        """Asynchronous APDU exchange with response

        Args:
            payload (bytes): APDU data to send

        Returns:
            Generator
        """

        with self.backend.exchange_async_raw(payload):
            yield


    def get_async_response(self) -> Optional[RAPDU]:
        """Asynchronous APDU response

        Returns:
            Response APDU
        """

        return self.backend.last_async_response


    def get_version(self) -> RAPDU:
        return self._exchange(self._cmd_builder.get_version())

    def get_serial(self) -> RAPDU:
        return self._exchange(self._cmd_builder.get_serial())


    def get_async_response(self) -> Optional[RAPDU]:
        return self.backend.last_async_response

    @contextmanager
    def get_pubkey_async(self, path: str) -> Generator[None, None, None]:
        with self._exchange_async(self._cmd_builder.get_pubkey_path(path)):
            yield


    @contextmanager
    def sign_opCert(self, testCase: OpCertTestCase) -> Generator[None, None, None]:
        """APDU Sign Operational Certificate

        Args:
            testCase (OpCertTestCase): Test parameters

        Returns:
            Generator
        """

        with self._exchange_async(self._cmd_builder.sign_opCert(testCase)):
            yield

    @contextmanager
    def sign_tx_witness_async(self, path: str) -> Generator[None, None, None]:
        """APDU Sign TX Witness

        Args:
            path (str): BIP44 derivation path

        Returns:
            Generator
        """

        with self._exchange_async(self._cmd_builder.sign_tx_witness(path)):
            yield

    def sign_tx_init_simple(self, options: int, tx_signing_mode: int, network_id: int,
                           protocol_magic: int, num_inputs: int, num_outputs: int, include_ttl: bool,
                           num_withdrawals: int = 0, include_validity_interval_start: bool = False,
                           num_witnesses: int = 0) -> RAPDU:
        """APDU Sign TX Init (simple chunked mode)

        Args:
            options (int): Transaction options (bit 0 = tagCborSets)
            tx_signing_mode (int): Transaction signing mode (3=ORDINARY, 4=POOL_OWNER, etc.)
            network_id (int): Network ID (0=testnet, 1=mainnet)
            protocol_magic (int): Protocol magic number
            num_inputs (int): Number of inputs
            num_outputs (int): Number of outputs
            include_ttl (bool): Whether TTL is included
            num_withdrawals (int): Number of withdrawals (default 0)
            include_validity_interval_start (bool): Whether validity interval start is included (default False)
            num_witnesses (int): Number of witnesses (default 0)

        Returns:
            Response APDU
        """
        data = bytearray()
        data.extend(options.to_bytes(8, 'big'))  # options as uint64
        data.append(tx_signing_mode)
        data.append(network_id)
        data.extend(protocol_magic.to_bytes(4, 'big'))
        data.extend(num_inputs.to_bytes(2, 'big'))
        data.extend(num_outputs.to_bytes(2, 'big'))
        data.extend(num_withdrawals.to_bytes(2, 'big'))  # num_withdrawals (2B)
        data.append(0x02 if include_ttl else 0x01)  # ITEM_INCLUDED_YES or ITEM_INCLUDED_NO
        data.append(0x02 if include_validity_interval_start else 0x01)  # validity interval start flag
        data.extend(num_witnesses.to_bytes(4, 'big'))  # num_witnesses (4B)

        from application_client.command_builder import P1Type
        # P1 = P1_TX_INIT for INIT APDU, P2 = P2_UNUSED
        return self._exchange(self._cmd_builder._serialize(InsType.SIGN_TX, P1Type.P1_TX_INIT, P1Type.P2_UNUSED, bytes(data)))

    def sign_tx_send_intermediate_chunks(self, tx):
        """Send all intermediate transaction chunks synchronously.

        This method sends all but the last chunk, catching any parsing errors immediately.
        Call this before entering the async context for the final chunk.

        Args:
            tx: Transaction object from signTx.py

        Returns:
            List of APDU chunks (all chunks for the transaction)
        """
        chunks = self._cmd_builder.serialize_transaction_chunks(tx)

        # Send all intermediate chunks synchronously
        for chunk in chunks[:-1]:
            response = self._exchange(chunk)
            if response.status != Errors.SW_SUCCESS:
                raise AssertionError(f"Intermediate chunk failed: {hex(response.status)}")

        return chunks

    @contextmanager
    def sign_tx_serialize_and_send_chunks_async(self, tx) -> Generator[None, None, None]:
        """Serialize transaction into chunks and send all with async on final chunk

        Sends all intermediate chunks synchronously, then the final chunk asynchronously.
        Use this when you need to navigate the UI during the final chunk processing.

        Args:
            tx: Transaction object from signTx.py

        Returns:
            Generator (use with 'with' statement for navigation)
        """
        chunks = self.sign_tx_send_intermediate_chunks(tx)

        # Send final chunk asynchronously (for UI navigation)
        with self._exchange_async(chunks[-1]):
            yield

    def sign_tx_witness(self, path: str) -> RAPDU:
        """APDU Sign TX Witness (synchronous)

        Args:
            path (str): BIP44 derivation path for witness

        Returns:
            Response APDU with signature
        """
        return self._exchange(self._cmd_builder.sign_tx_witness(path))

