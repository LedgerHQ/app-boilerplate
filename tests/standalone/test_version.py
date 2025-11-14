# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for application version and serial information.
Tests both OS-level and app-level version retrieval mechanisms.
"""

from ragger.utils.misc import get_current_app_name_and_version
from ragger.backend import BackendInterface

from application_client.command_sender import CommandSender
from application_client.response_unpacker import unpack_get_version_response, unpack_get_serial_response

from .utils import verify_name, verify_version


def test_check_version(backend: BackendInterface) -> None:
    """Check version and name returned by the OS (BOLOS-level)."""
    # This tests the OS-level version check, independent of the app
    app_name, version = get_current_app_name_and_version(backend)
    print(f" Name: {app_name}")
    print(f" Version: {version}")
    verify_name(app_name)
    verify_version(version)


def test_check_app_version(backend: BackendInterface) -> None:
    """Check version returned by the app via GET_VERSION APDU."""
    # Use the app interface to send GET_VERSION command
    client = CommandSender(backend)
    rapdu = client.get_version()

    # Parse the version response using the unpacker
    major, minor, patch = unpack_get_version_response(rapdu.data)
    vers_str = f"{major}.{minor}.{patch}"

    print(f" Version: {vers_str}")
    verify_version(vers_str)


def test_check_app_serial(backend: BackendInterface) -> None:
    """Check application serial number."""
    # Use the app interface to send GET_SERIAL command
    client = CommandSender(backend)
    rapdu = client.get_serial()

    # Parse the serial response using the unpacker
    serial = unpack_get_serial_response(rapdu.data)

    print(f" Serial: {serial.hex()} -> {serial.decode()}")
