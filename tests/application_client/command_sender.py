from typing import Generator, Optional
from contextlib import contextmanager

from ragger.backend.interface import BackendInterface, RAPDU

from standalone.input_files.signOpCert import OpCertTestCase
from application_client.command_builder import CommandBuilder
from application_client.status_words import StatusWord


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
                           num_certificates: int = 0, num_withdrawals: int = 0, include_validity_interval_start: bool = False,
                           num_mint_asset_groups: int = 0, num_witnesses: int = 0, num_voters: int = 0) -> RAPDU:
        """APDU Sign TX Init (simple chunked mode)

        Args:
            options (int): Transaction options (bit 0 = tagCborSets)
            tx_signing_mode (int): Transaction signing mode (3=ORDINARY, 4=POOL_OWNER, etc.)
            network_id (int): Network ID (0=testnet, 1=mainnet)
            protocol_magic (int): Protocol magic number
            num_inputs (int): Number of inputs
            num_outputs (int): Number of outputs
            include_ttl (bool): Whether TTL is included
            num_certificates (int): Number of certificates (default 0)
            num_withdrawals (int): Number of withdrawals (default 0)
            include_validity_interval_start (bool): Whether validity interval start is included (default False)
            num_mint_asset_groups (int): Number of mint asset groups (default 0)
            num_witnesses (int): Number of witnesses (default 0)
            num_voters (int): Number of voters in voting procedures (default 0)

        Returns:
            Response APDU
        """
        data = bytearray()

        # Fixed header: options, networkId, protocolMagic, signingMode
        data.extend(options.to_bytes(8, 'big'))
        data.append(network_id)
        data.extend(protocol_magic.to_bytes(4, 'big'))
        data.append(tx_signing_mode)

        # Fields 0-1: inputs and outputs (always present)
        data.extend(num_inputs.to_bytes(2, 'big'))
        data.extend(num_outputs.to_bytes(2, 'big'))

        # Field 3 (TTL) - optional
        data.append(0x02 if include_ttl else 0x01)
        # Field 4 (certificates) - optional
        data.extend(num_certificates.to_bytes(2, 'big'))
        # Field 5 (withdrawals) - optional
        data.extend(num_withdrawals.to_bytes(2, 'big'))

        # Field 7 (auxiliary data hash) - optional, always false for now
        data.append(0x01)
        # Field 8 (validity interval start) - optional
        data.append(0x02 if include_validity_interval_start else 0x01)

        # Field 9 (mint) - optional
        data.extend(num_mint_asset_groups.to_bytes(2, 'big'))

        # Field 11 (script data hash) - optional, always false for now
        data.append(0x01)
        # Field 13 (collateral inputs) - optional, always 0 for now
        data.extend((0).to_bytes(2, 'big'))
        # Field 14 (required signers) - optional, always 0 for now
        data.extend((0).to_bytes(2, 'big'))
        # Field 15 (network ID) - optional, always false for now
        data.append(0x01)
        # Field 16 (collateral output) - optional, always false for now
        data.append(0x01)
        # Field 17 (total collateral) - optional, always false for now
        data.append(0x01)
        # Field 18 (reference inputs) - optional, always 0 for now
        data.extend((0).to_bytes(2, 'big'))
        # Field 19 (voting procedures) - optional
        data.extend(num_voters.to_bytes(2, 'big'))
        # Field 21 (treasury) - optional, always false for now
        data.append(0x01)
        # Field 22 (donation) - optional, always false for now
        data.append(0x01)

        # Number of witness paths
        data.extend(num_witnesses.to_bytes(2, 'big'))

        return self._exchange(self._cmd_builder.sign_tx_init_simple(
            options=options,
            tx_signing_mode=tx_signing_mode,
            network_id=network_id,
            protocol_magic=protocol_magic,
            num_inputs=num_inputs,
            num_outputs=num_outputs,
            include_ttl=include_ttl,
            num_certificates=num_certificates,
            num_withdrawals=num_withdrawals,
            include_validity_interval_start=include_validity_interval_start,
            num_mint_asset_groups=num_mint_asset_groups,
            num_witnesses=num_witnesses,
            num_voters=num_voters,
        ))

    @contextmanager
    def sign_tx_send_chunks(self, tx) -> Generator[None, None, None]:
        """Serialize transaction into chunks and send them.

        Sends all intermediate chunks synchronously, then the final chunk asynchronously
        for UI navigation.

        Args:
            tx: Transaction object from signTx.py

        Returns:
            Generator (use with 'with' statement for navigation)
        """
        chunks = self._cmd_builder.serialize_transaction_chunks(tx)

        # Send all intermediate chunks synchronously
        for chunk in chunks[:-1]:
            response = self._exchange(chunk)
            if response.status != StatusWord.SWO_SUCCESS:
                raise AssertionError(f"Intermediate chunk failed: {hex(response.status)}")

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
