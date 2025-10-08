# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Version/Serial check
"""

from ragger.utils.misc import get_current_app_name_and_version
from ragger.backend import BackendInterface

from application_client.command_sender import CommandSender

from standalone.utils import verify_name, verify_version


def test_check_version(backend: BackendInterface) -> None:
    """Check version and name, returned by the OS"""

    # Send the APDU
    app_name, version = get_current_app_name_and_version(backend)
    print(f" Name: {app_name}")
    print(f" Version: {version}")
    verify_name(app_name)
    verify_version(version)


def test_check_app_version(backend: BackendInterface) -> None:
    """Check version and name, returned by the App"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)
    # Send the APDU
    version = client.get_version()

    print(f" Version: {version.hex()}")
    vers_str = f"{version[0]}.{version[1]}.{version[2]}"
    verify_version(vers_str)


def test_check_app_serial(backend: BackendInterface) -> None:
    """Check App Serial"""

    # Use the app interface instead of raw interface
    client = CommandSender(backend)
    # Send the APDU
    serial = client.get_serial()

    print(f" Serial: {serial.hex()} -> {serial.decode()}")
