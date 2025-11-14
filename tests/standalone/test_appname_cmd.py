# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for application name verification.
"""

from ragger.backend.interface import BackendInterface

from application_client.command_sender import CommandSender
from application_client.response_unpacker import unpack_get_app_name_response

from .utils import verify_name


def test_app_name(backend: BackendInterface) -> None:
    """Check application name via GET_APP_NAME APDU."""
    client = CommandSender(backend)
    response = client.get_app_name()
    app_name = unpack_get_app_name_response(response.data)
    verify_name(app_name)
