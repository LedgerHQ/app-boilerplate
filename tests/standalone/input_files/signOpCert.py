# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Sign Operational Certificate
"""

from dataclasses import dataclass


@dataclass
class operationalCertificate:
    kesPublicKeyHex: str
    kesPeriod: int
    issueCounter: int
    path: str

@dataclass
class OpCertTestCase:
    name: str
    opCert: operationalCertificate
    warning: bool = False


# pylint: disable=line-too-long
opCertTestCases = [
    OpCertTestCase("Should_correctly_sign_operational_certificate",
                   operationalCertificate("3d24bc547388cf2403fd978fc3d3a93d1f39acf68a9c00e40512084dc05f2822",
                                          47,
                                          42,
                                          "m/1853'/1815'/0'/0'")
    ),
    OpCertTestCase("Should_correctly_sign_operational_certificate_with_warning",
                   operationalCertificate("3d24bc547388cf2403fd978fc3d3a93d1f39acf68a9c00e40512084dc05f2822",
                                          47,
                                          42,
                                          "m/1853'/1815'/0'/1000001'"),
                   warning = True
    )
]
