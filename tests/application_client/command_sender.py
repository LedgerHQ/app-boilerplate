from typing import Callable, Generator, List, Optional, Tuple
from contextlib import contextmanager

from ragger.backend.interface import BackendInterface, RAPDU


from standalone.input_files.signOpCert import OpCertTestCase
from application_client.command_builder import CommandBuilder, gather_witness_paths, P1Type
from standalone.input_files.derive_address import DeriveAddressTestCase
from application_client.status_words import StatusWord
from standalone.input_files.signTx import Transaction, TxAuxiliaryDataCIP36, TxAuxiliaryDataType



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

    def sign_tx(self,
                tx: Transaction,
                signing_mode: int,
                additional_witness_paths: Optional[List[str]] = None,
                options: int = 0,
                on_review: Optional[Callable[[], None]] = None) -> Tuple[bytes, List[str]]:
        """Sign a transaction and return its hash plus the witness paths used.

        This builds the init APDU from the transaction body, sends the raw chunks,
        and waits for the final response after the user approves the transaction.
        """
        extra_paths = additional_witness_paths or []
        witness_paths = gather_witness_paths(tx, signing_mode, extra_paths)
        if not witness_paths:
            raise AssertionError("No witness paths found in transaction")

        init_params = self._cmd_builder.build_tx_init_params(
            tx=tx,
            signing_mode=signing_mode,
            witness_paths=witness_paths,
            options=options,
        )
        response = self._exchange(self._cmd_builder.sign_tx_init(init_params))
        if response.status != StatusWord.SWO_SUCCESS:
            raise AssertionError(f"Init failed: {hex(response.status)}")

        self._send_tx_aux_data_if_present(tx)

        with self.sign_tx_send_chunks(tx):
            if on_review is not None:
                on_review()

        response = self.get_async_response()
        if response is None:
            raise AssertionError("No response from final chunk")
        if response.status != StatusWord.SWO_SUCCESS:
            raise AssertionError(f"Transaction failed: {hex(response.status)}")

        return response.data, witness_paths

    def _send_tx_aux_data_if_present(self, tx: Transaction) -> None:
        if tx.auxiliaryData is None:
            return
        if tx.auxiliaryData.type != TxAuxiliaryDataType.CIP36_REGISTRATION:
            return

        aux_params = tx.auxiliaryData.params
        if not isinstance(aux_params, TxAuxiliaryDataCIP36):
            raise AssertionError("Unexpected auxiliary data params type")

        response = self._exchange(self._cmd_builder.sign_tx_aux_data_init(tx, aux_params))
        if response.status != StatusWord.SWO_SUCCESS:
            raise AssertionError(f"AUX_DATA init failed: {hex(response.status)}")

        for delegation in aux_params.delegations:
            response = self._exchange(self._cmd_builder.sign_tx_aux_data_delegation(delegation))
            if response.status != StatusWord.SWO_SUCCESS:
                raise AssertionError(f"AUX_DATA registration failed: {hex(response.status)}")

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

<<<<<<< HEAD
    def set_debug_settings(self, expert_mode: bool, silent_export: bool) -> RAPDU:
        """Set app settings via debug APDU (only works with DEBUG builds).

        This is a debug-only command that allows tests to programmatically set
        app settings without UI navigation. It only works when the app is built
        with DEBUG=1 flag.

        Args:
            expert_mode: True to enable expert mode, False to disable
            silent_export: True to enable silent pubkey export, False to disable

        Returns:
            Response APDU with current settings as confirmation (2 bytes)

        Raises:
            AssertionError: If the command fails or returns unexpected status
        """
        response = self._exchange(self._cmd_builder.debug_set_settings(expert_mode, silent_export))

        if response.status != StatusWord.SWO_SUCCESS:
            raise AssertionError(f"Debug set settings failed: {hex(response.status)}")

        # Verify response contains 2 bytes (current settings)
        if len(response.data) != 2:
            raise AssertionError(f"Expected 2 bytes in response, got {len(response.data)}")

        # Verify settings were applied correctly
        actual_expert = response.data[0]
        actual_silent = response.data[1]
        expected_expert = 0x01 if expert_mode else 0x00
        expected_silent = 0x01 if silent_export else 0x00

        if actual_expert != expected_expert or actual_silent != expected_silent:
            raise AssertionError(
                f"Settings mismatch: expected expert={expected_expert}, silent={expected_silent}, "
                f"got expert={actual_expert}, silent={actual_silent}"
            )

        return response
=======
    @contextmanager
    def derive_address_async(self, p1: P1Type, testCase: DeriveAddressTestCase) -> Generator[None, None, None]:
        """APDU Derive Address

        Args:
            p1 (P1Type): APDU Parameter 1
            testCase (DeriveAddressTestCase): Test parameters

        Returns:
            Generator
        """

        with self._exchange_async(self._cmd_builder.derive_address(p1, testCase)):
            yield

    def derive_address(self, p1: P1Type, testCase: DeriveAddressTestCase) -> RAPDU:
        """APDU Derive Address

        Args:
            p1 (P1Type): APDU Parameter 1
            testCase (DeriveAddressTestCase): Test parameters

        Returns:
            Response APDU
        """

        return self._exchange(self._cmd_builder.derive_address(p1, testCase))
>>>>>>> 1f05ffa (derive address tests)
