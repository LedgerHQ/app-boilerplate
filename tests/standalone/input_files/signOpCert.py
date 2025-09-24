# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2024 Ledger SAS
# SPDX-License-Identifier: LicenseRef-LEDGER
"""
This module provides Ragger tests for Sign Operational Certificate
"""

from dataclasses import dataclass


@dataclass
class OperationalCertificateSignature:
    signatureHex: str

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
    expected: OperationalCertificateSignature
    warning: bool = False


# pylint: disable=line-too-long
opCertTestCases = [
    OpCertTestCase("Should_correctly_sign_operational_certificate",
                   operationalCertificate("3d24bc547388cf2403fd978fc3d3a93d1f39acf68a9c00e40512084dc05f2822",
                                          47,
                                          42,
                                          "m/1853'/1815'/0'/0'"),
                   OperationalCertificateSignature("ce8d7cab55217ed17f1cceb8cb487dcbe6172fdb5794cc26f78c2f1d2495598e72beb6209f113562f9488ef6e81e3e8f758ea072c3cf9c17095868f2e9213f0a")
    ),
    OpCertTestCase("Should_correctly_sign_operational_certificate_with_warning",
                   operationalCertificate("3d24bc547388cf2403fd978fc3d3a93d1f39acf68a9c00e40512084dc05f2822",
                                          47,
                                          42,
                                          "m/1853'/1815'/0'/1000001'"),
                   OperationalCertificateSignature("9f926e85b8f8cd124c0977504fa5c06361beb26774f14628cd2913cf9baf8d9a5c2c55835fa8fab723e1a9d2d7ea23944bb96a811b7bb6cce4bfabb778fc9308"),
                   warning = True
    )
]
