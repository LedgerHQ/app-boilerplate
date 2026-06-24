#!/usr/bin/env python3
"""
ML-DSA APDU Test Script

Tests ML-DSA key generation, signature generation, and signature verification
over APDU using the Ledger device. Supports ML-DSA-44, ML-DSA-65, and ML-DSA-87
parameter sets.

APDU P2 encoding:
    - bits 0-1: parameter set (0=ML-DSA-44, 1=ML-DSA-65, 2=ML-DSA-87)
    - bit 7: more flag (0=last chunk, 1=more chunks)

APDU input formats:
  - keyGen:  seed (32 bytes)
    - sign:    sk || mu (mu is computed from M' unless --mu is set)
    - verify:  pk || sig || mu (mu is computed from M' unless --mu is set)

For external/pure mode, M' = 0x00 || len(ctx) || ctx || msg.
For external/preHash mode, M' = 0x01 || len(ctx) || ctx || OID || PH(msg).
For internal mode, M' = message (raw internal message bytes).
"""
import argparse
import json
import re
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass, field
from typing import Dict, List, Tuple

CLA = 0xE0
INS_MLDSA_KEYGEN = 0x40
INS_MLDSA_SIGN = 0x41
INS_MLDSA_VERIFY = 0x42
SW_SUCCESS = 0x9000
MAX_APDU_DATA_LEN = 255
MLDSA_SEEDBYTES = 32
MLDSA_CRHBYTES = 64
MLDSA_SK_TR_OFF = 64

# P2 encoding: bits 0-1 = param set, bit 7 = more flag
P2_MORE_FLAG = 0x80
P2_PARAM_44 = 0x00
P2_PARAM_65 = 0x01
P2_PARAM_87 = 0x02

# Parameter set sizes: (seed_bytes, pk_bytes, sk_bytes, sig_bytes)
MLDSA_SIZES: Dict[str, Tuple[int, int, int, int]] = {
    "ML-DSA-44": (32, 1312, 2560, 2420),
    "ML-DSA-65": (32, 1952, 4032, 3309),
    "ML-DSA-87": (32, 2592, 4896, 4627),
}

# Estimated device RAM usage per operation (bytes).
#
# Derived from the BOLOS SDK implementation ($BOLOS_SDK/lib_cxng/src/cx_mldsa.c):
#
#   sizeof(mldsa_poly)    = MLDSA_N * 4            = 1024 bytes
#   sizeof(mldsa_polyvecl)= MLDSA_MAX_L * 1024     (MAX_L = 7 w/ HAVE_MLDSA_87, else 5)
#   sizeof(mldsa_polyveck)= MLDSA_MAX_K * 1024     (MAX_K = 8 w/ HAVE_MLDSA_87, else 6)
#
#   MLDSA_keygen_internal stack:
#     seedbuf(128) + buf(34) + tr(64)
#     + s1(polyvecl) + t(polyveck) + tmp_poly(1024) + t0_row(1024)
#     = 2274 + (MAX_K + MAX_L) * 1024
#
#   mldsa_sign_core stack  (MLDSA_sign_stack_workspace_t + zero_rnd):
#     rho(32) + mu(64) + rhoprime(64) + ctilde(64) + zero_rnd(32)
#     + overlay.loop_phase:
#         y(polyvecl) + yhat(polyvecl) + w(polyveck) + w1(polyveck)
#         + cp(1024) + tmp(1024) + skpoly(1024) + w1_packed(MAX_K*192)
#     = 3328 + 2*(MAX_L + MAX_K)*1024 + MAX_K*192
#
#   mldsa_verify_core stack (MLDSA_verify_stack_workspace_t):
#     rho(32) + ctilde(64) + mu(64) + ctilde2(64)
#     + cp(1024) + ztmp(1024) + t1tmp(1024) + htmp(1024)
#     + w1_packed(MAX_K*192)
#     + overlay.az_phase: aij(1024) + dot(1024) = 2048
#     = 6368 + MAX_K*192
#
#   App-level global buffer (G_mldsa_ctx.buf):  7341 bytes  (MLDSA_APDU_BUF_SIZE)
#
# The estimates below include the SDK stack workspace + the app APDU buffer.

_POLY = 1024                   # sizeof(mldsa_poly) = MLDSA_N * sizeof(int32_t)
_APDU_BUF = 7341               # G_mldsa_ctx.buf (MLDSA_APDU_BUF_SIZE)

def _compute_memory_estimates(max_k: int, max_l: int) -> Dict[str, Dict[str, int]]:
    """Compute per-operation RAM estimates for a given SDK MAX_K/MAX_L config."""
    keygen_stack = 2274 + (max_k + max_l) * _POLY
    sign_stack   = 3328 + 2 * (max_l + max_k) * _POLY + max_k * 192
    verify_stack = 6368 + max_k * 192
    entry = {
        "keygen": keygen_stack + _APDU_BUF,
        "sign":   sign_stack   + _APDU_BUF,
        "verify": verify_stack + _APDU_BUF,
    }
    # Same struct sizes for all param sets (MAX_K/MAX_L are compile-time constants)
    return {ps: dict(entry) for ps in MLDSA_SIZES}

# With HAVE_MLDSA_87: MAX_K=8, MAX_L=7
MLDSA_MEMORY_ESTIMATE: Dict[str, Dict[str, int]] = _compute_memory_estimates(max_k=8, max_l=7)
# Without HAVE_MLDSA_87: MAX_K=6, MAX_L=5 — uncomment to use:
# MLDSA_MEMORY_ESTIMATE = _compute_memory_estimates(max_k=6, max_l=5)

INS_TO_OP_NAME: Dict[int, str] = {
    INS_MLDSA_KEYGEN: "keygen",
    INS_MLDSA_SIGN: "sign",
    INS_MLDSA_VERIFY: "verify",
}

# Map parameter set name to P2 value
PARAM_SET_P2: Dict[str, int] = {
    "ML-DSA-44": P2_PARAM_44,
    "ML-DSA-65": P2_PARAM_65,
    "ML-DSA-87": P2_PARAM_87,
}


@dataclass
class Rapdu:
    data: bytes
    sw: int


@dataclass
class PerfRecord:
    """Stores timing and size information for a single ML-DSA operation."""
    param_set: str
    operation: str
    input_bytes: int
    output_bytes: int
    elapsed_s: float
    label: str
    est_memory: int = 0
    compute_s: float = 0.0


perf_records: List[PerfRecord] = []


def normalize_hex(value: str) -> str:
    return re.sub(r"[^0-9a-fA-F]", "", value).upper()


def compute_mu(tr: bytes, mprime: bytes) -> bytes:
    """Compute mu = SHAKE256(tr || M', 64)."""
    from hashlib import shake_256
    return shake_256(tr + mprime).digest(MLDSA_CRHBYTES)


def compute_mu_from_sk(sk: bytes, mprime: bytes) -> bytes:
    """Extract tr from sk and compute mu = SHAKE256(tr || M', 64)."""
    tr = sk[MLDSA_SK_TR_OFF:MLDSA_SK_TR_OFF + MLDSA_CRHBYTES]
    return compute_mu(tr, mprime)


def compute_mu_from_pk(pk: bytes, mprime: bytes) -> bytes:
    """Compute tr = SHAKE256(pk, 64), then mu = SHAKE256(tr || M', 64)."""
    from hashlib import shake_256
    tr = shake_256(pk).digest(MLDSA_CRHBYTES)
    return compute_mu(tr, mprime)


def parse_hex_arg(name: str, value: str, expected_len: int) -> bytes:
    cleaned = normalize_hex(value)
    if len(cleaned) != expected_len * 2:
        raise ValueError(
            f"{name} must be exactly {expected_len} bytes ({expected_len * 2} hex chars), "
            f"got {len(cleaned) // 2} bytes"
        )
    return bytes.fromhex(cleaned)


def parse_rapdu(stdout: str) -> Rapdu:
    text = stdout.strip()

    # Device (HID) format: "HID <= <data_hex><sw_hex>"
    hid_match = re.search(r"HID\s*<=\s*([0-9A-Fa-f]+)", text)
    if hid_match:
        full_hex = hid_match.group(1)
        if len(full_hex) >= 4 and len(full_hex) % 2 == 0:
            sw = int(full_hex[-4:], 16)
            data_hex = full_hex[:-4]
            data = bytes.fromhex(data_hex) if data_hex else b""
            return Rapdu(data=data, sw=sw)

    sw_match = re.search(r"SW\s*[:=]\s*([0-9A-Fa-f]{4})", text)
    if sw_match:
        sw = int(sw_match.group(1), 16)
        data_match = re.search(r"DATA\s*[:=]\s*([0-9A-Fa-f\s]+)", text)
        if data_match:
            data_hex = normalize_hex(data_match.group(1))
            data = bytes.fromhex(data_hex) if data_hex else b""
            return Rapdu(data=data, sw=sw)

    # ledgerblue common format, e.g. b'<hex_data>'9000
    literal_with_sw = list(
        re.finditer(r"b['\"]([0-9A-Fa-f]*)['\"]\s*['\"]?([0-9A-Fa-f]{4})['\"]?", text, re.DOTALL)
    )
    if literal_with_sw:
        last = literal_with_sw[-1]
        data_hex = last.group(1)
        sw = int(last.group(2), 16)
        data = bytes.fromhex(data_hex) if data_hex else b""
        return Rapdu(data=data, sw=sw)

    literal_matches = list(re.finditer(r"b['\"]([0-9A-Fa-f]*)['\"]", text, re.DOTALL))
    if literal_matches:
        last = literal_matches[-1]
        data_hex = last.group(1)
        tail = text[last.end():]

        sw_after = re.search(r"(?:^|\n|\r)\s*['\"]?([0-9A-Fa-f]{4})['\"]?\s*$", tail)
        if sw_after:
            data = bytes.fromhex(data_hex) if data_hex else b""
            sw = int(sw_after.group(1), 16)
            return Rapdu(data=data, sw=sw)

        if len(data_hex) >= 4 and len(data_hex) % 2 == 0:
            sw_candidate = int(data_hex[-4:], 16)
            if (sw_candidate & 0xF000) in (0x6000, 0x9000, 0xB000, 0xC000):
                data = bytes.fromhex(data_hex[:-4]) if len(data_hex) > 4 else b""
                return Rapdu(data=data, sw=sw_candidate)

    # Fallback: parse a plain RAPDU hex stream and split data||SW.
    hex_tokens = [token for token in re.findall(r"[0-9A-Fa-f]+", text) if len(token) % 2 == 0]
    combined = "".join(hex_tokens)
    if len(combined) < 4:
        raise ValueError(f"Could not parse RAPDU from command output: {stdout}")

    sw = int(combined[-4:], 16)
    data_hex = combined[:-4]
    data = bytes.fromhex(data_hex) if data_hex else b""
    return Rapdu(data=data, sw=sw)


def send_apdu(ledgerblue_cmd: str, apdu_hex: str) -> Rapdu:
    cmd = f"echo {shlex.quote(apdu_hex)} | {ledgerblue_cmd}"
    proc = subprocess.run(["bash", "-lc", cmd], check=False, capture_output=True, text=True)

    if proc.returncode != 0:
        raise RuntimeError(
            "APDU command failed\n"
            f"CMD: {cmd}\n"
            f"STDOUT: {proc.stdout}\n"
            f"STDERR: {proc.stderr}"
        )

    rapdu = parse_rapdu(proc.stdout)
    if rapdu.sw != SW_SUCCESS:
        raise RuntimeError(
            f"Device returned non-success SW=0x{rapdu.sw:04X} for APDU {apdu_hex}"
        )

    return rapdu


def build_apdu(ins: int, p1: int, p2: int, data: bytes) -> str:
    lc = len(data)
    return f"{CLA:02X}{ins:02X}{p1:02X}{p2:02X}{lc:02X}" + data.hex().upper()


def split_chunks(payload: bytes, chunk_size: int = MAX_APDU_DATA_LEN) -> list[bytes]:
    if len(payload) == 0:
        raise ValueError("input payload must not be empty")
    return [payload[i:i + chunk_size] for i in range(0, len(payload), chunk_size)]


def run_operation(
    ledgerblue_cmd: str,
    ins: int,
    input_data: bytes,
    expected: bytes,
    label: str,
    verbose: bool,
    param_set: str = "ML-DSA-44",
) -> int:
    """Execute an ML-DSA operation over APDU with chunked input/output."""
    if param_set not in PARAM_SET_P2:
        raise ValueError(f"Unknown parameter set: {param_set}")

    t_start = time.monotonic()

    p2_base = PARAM_SET_P2[param_set]
    input_chunks = split_chunks(input_data)
    input_p1 = 0

    for chunk in input_chunks[:-1]:
        p2 = p2_base | P2_MORE_FLAG
        apdu = build_apdu(ins=ins, p1=input_p1, p2=p2, data=chunk)
        if verbose:
            print(f"[send] {apdu}")
        rapdu_ack = send_apdu(ledgerblue_cmd, apdu)
        if verbose:
            print(f"[recv] input-ack p1={input_p1} data={len(rapdu_ack.data)}")
        input_p1 += 1
        if input_p1 > 255:
            raise RuntimeError("Too many input chunks (P1 overflow)")

    # Last input chunk: more flag = 0
    # Start compute timer here: the device performs the ML-DSA operation
    # upon receiving the final input chunk and responds with the first output chunk.
    p2 = p2_base
    final_apdu = build_apdu(ins=ins, p1=input_p1, p2=p2, data=input_chunks[-1])
    if verbose:
        print(f"[send] {final_apdu}")
    t_compute_start = time.monotonic()
    rapdu = send_apdu(ledgerblue_cmd, final_apdu)
    t_compute = time.monotonic() - t_compute_start

    if len(rapdu.data) < 2:
        raise RuntimeError("First response chunk too short: missing TOTAL_LEN header")

    total_len = int.from_bytes(rapdu.data[:2], "big")
    received = bytearray(rapdu.data[2:])

    if verbose:
        print(f"[recv] chunk=0 total_len={total_len} data={len(received)}")

    response_p1 = input_p1 + 1
    response_chunks = 1
    while len(received) < total_len:
        next_apdu = build_apdu(ins=ins, p1=response_p1, p2=p2_base, data=b"")
        if verbose:
            print(f"[send] {next_apdu}")

        rapdu = send_apdu(ledgerblue_cmd, next_apdu)
        received.extend(rapdu.data)

        if verbose:
            print(f"[recv] chunk={response_p1} +{len(rapdu.data)} => {len(received)}/{total_len}")

        response_p1 += 1
        response_chunks += 1
        if response_p1 > 255:
            raise RuntimeError("Too many response chunks (P1 overflow)")

    if len(received) != total_len:
        raise RuntimeError(
            f"Reassembled response length mismatch: got {len(received)}, expected {total_len}"
        )

    if total_len != len(expected):
        raise RuntimeError(
            f"TOTAL_LEN mismatch: device={total_len}, expected={len(expected)}"
        )

    elapsed = time.monotonic() - t_start

    if bytes(received) != expected:
        mismatch_at = next(
            (idx for idx, (a, b) in enumerate(zip(received, expected)) if a != b),
            None,
        )
        if mismatch_at is None and len(received) != len(expected):
            mismatch_msg = "length differs"
        else:
            mismatch_msg = f"first diff at byte {mismatch_at}"
        raise RuntimeError(f"KAT mismatch: response != expected {label} ({mismatch_msg})")

    op_name = INS_TO_OP_NAME.get(ins, "unknown")
    est_mem = MLDSA_MEMORY_ESTIMATE.get(param_set, {}).get(op_name, 0)
    record = PerfRecord(
        param_set=param_set,
        operation=op_name,
        input_bytes=len(input_data),
        output_bytes=total_len,
        elapsed_s=elapsed,
        label=label,
        est_memory=est_mem,
        compute_s=t_compute,
    )
    perf_records.append(record)

    print(f"KAT OK [{param_set}]: {label} response matches expected {label} (total {elapsed:.3f}s, compute {t_compute:.3f}s)")
    print(f"  total_len={total_len}, input_chunks={len(input_chunks)}, response_chunks={response_chunks}")
    return 0


def build_mprime_external(pre_hash: str, context: bytes, message: bytes, hash_alg: str) -> bytes:
    """Construct M' for external signature interface (FIPS 204 Section 5.4).

    Pure mode:    M' = 0x00 || len(ctx) || ctx || msg
    PreHash mode: M' = 0x01 || len(ctx) || ctx || oid(PH) || PH(M, n)
    """
    if pre_hash == "pure":
        return bytes([0x00, len(context)]) + context + message
    elif pre_hash == "preHash":
        import hashlib

        # FIPS 204 Table 2: DER-encoded OIDs and output lengths (n in bytes)
        HASH_PARAMS: Dict[str, Tuple[bytes, str, int]] = {
            "SHA2-224":     (bytes.fromhex("0609608648016503040204"), "sha224",      28),
            "SHA2-256":     (bytes.fromhex("0609608648016503040201"), "sha256",      32),
            "SHA2-384":     (bytes.fromhex("0609608648016503040202"), "sha384",      48),
            "SHA2-512":     (bytes.fromhex("0609608648016503040203"), "sha512",      64),
            "SHA2-512/224": (bytes.fromhex("0609608648016503040205"), "sha512_224",  28),
            "SHA2-512/256": (bytes.fromhex("0609608648016503040206"), "sha512_256",  32),
            "SHA3-224":     (bytes.fromhex("0609608648016503040207"), "sha3_224",    28),
            "SHA3-256":  (bytes.fromhex("0609608648016503040208"), "sha3_256",  32),
            "SHA3-384":  (bytes.fromhex("0609608648016503040209"), "sha3_384",  48),
            "SHA3-512":  (bytes.fromhex("060960864801650304020a"), "sha3_512",  64),
            "SHAKE-128": (bytes.fromhex("060960864801650304020b"), "shake_128", 32),
            "SHAKE-256": (bytes.fromhex("060960864801650304020c"), "shake_256", 64),
        }

        if hash_alg not in HASH_PARAMS:
            raise ValueError(f"Unsupported hashAlg for preHash: {hash_alg}")

        oid, func_name, n_bytes = HASH_PARAMS[hash_alg]
        h = hashlib.new(func_name)
        h.update(message)

        if hash_alg.startswith("SHAKE"):
            ph = h.digest(n_bytes)
        else:
            ph = h.digest()

        return bytes([0x01, len(context)]) + context + oid + ph
    else:
        raise ValueError(f"Unknown preHash mode: {pre_hash}")


def run_json(
    ledgerblue_cmd: str,
    json_path: str,
    verbose: bool,
    param_filter: str = None,
    tc_id: int = None,
) -> int:
    """Run ML-DSA test vectors from an ACVP JSON file.

    Supports keyGen, sigGen, and sigVer modes.
    """
    with open(json_path) as f:
        data = json.load(f)

    mode = data.get("mode")
    passed = 0
    failed = 0
    skipped = 0

    for group in data.get("testGroups", []):
        param_set = group.get("parameterSet")
        if param_set not in MLDSA_SIZES:
            if verbose:
                print(f"Skipping unsupported parameter set: {param_set}")
            continue

        if param_filter and param_set != param_filter:
            continue

        seed_bytes, pk_bytes, sk_bytes, sig_bytes = MLDSA_SIZES[param_set]
        sig_interface = group.get("signatureInterface", "")
        pre_hash = group.get("preHash", "")
        deterministic = group.get("deterministic", True)

        for test in group.get("tests", []):
            cur_tc_id = test.get("tcId", "?")

            if tc_id is not None and cur_tc_id != tc_id:
                continue

            try:
                if mode == "keyGen":
                    seed = bytes.fromhex(test["seed"])
                    pk = bytes.fromhex(test["pk"])
                    sk = bytes.fromhex(test["sk"])

                    if len(seed) != seed_bytes:
                        raise ValueError(
                            f"Seed size mismatch: got {len(seed)}, expected {seed_bytes}"
                        )
                    if len(pk) != pk_bytes or len(sk) != sk_bytes:
                        raise ValueError(
                            f"Key size mismatch for {param_set}: "
                            f"pk={len(pk)} (expected {pk_bytes}), "
                            f"sk={len(sk)} (expected {sk_bytes})"
                        )

                    label = f"keygen tcId={cur_tc_id}"
                    run_operation(
                        ledgerblue_cmd=ledgerblue_cmd,
                        ins=INS_MLDSA_KEYGEN,
                        input_data=seed,
                        expected=pk + sk,
                        label=label,
                        verbose=verbose,
                        param_set=param_set,
                    )

                elif mode == "sigGen":
                    sk = bytes.fromhex(test["sk"])
                    sig = bytes.fromhex(test["signature"])
                    external_mu = test.get("externalMu", group.get("externalMu", False))

                    if len(sk) != sk_bytes:
                        raise ValueError(f"sk size mismatch: got {len(sk)}, expected {sk_bytes}")
                    if len(sig) != sig_bytes:
                        raise ValueError(f"sig size mismatch: got {len(sig)}, expected {sig_bytes}")

                    # Non-deterministic sigGen uses rnd; skip if device only supports deterministic
                    if not deterministic and "rnd" in test:
                        if verbose:
                            print(
                                f"Skipping non-deterministic sigGen tcId={cur_tc_id} "
                                f"(tgId={group.get('tgId', '?')})"
                            )
                        skipped += 1
                        continue

                    if external_mu:
                        mu = bytes.fromhex(test["mu"])
                    elif sig_interface == "external":
                        msg = bytes.fromhex(test["message"])
                        ctx = bytes.fromhex(test.get("context", ""))
                        hash_alg = test.get("hashAlg", "none")
                        mprime = build_mprime_external(pre_hash, ctx, msg, hash_alg)
                        mu = compute_mu_from_sk(sk, mprime)
                    elif sig_interface == "internal":
                        mprime = bytes.fromhex(test["message"])
                        mu = compute_mu_from_sk(sk, mprime)
                    else:
                        print(f"Skipping unsupported signatureInterface: {sig_interface}")
                        skipped += 1
                        continue

                    label = f"siggen tcId={cur_tc_id} ({sig_interface}/{pre_hash})"
                    run_operation(
                        ledgerblue_cmd=ledgerblue_cmd,
                        ins=INS_MLDSA_SIGN,
                        input_data=sk + mu,
                        expected=sig,
                        label=label,
                        verbose=verbose,
                        param_set=param_set,
                    )

                elif mode == "sigVer":
                    pk = bytes.fromhex(test["pk"])
                    sig = bytes.fromhex(test["signature"])
                    test_passed = test.get("testPassed", True)
                    external_mu = test.get("externalMu", group.get("externalMu", False))

                    if len(pk) != pk_bytes:
                        raise ValueError(f"pk size mismatch: got {len(pk)}, expected {pk_bytes}")
                    if len(sig) != sig_bytes:
                        # Some sigVer vectors have modified signatures (wrong length)
                        # that should fail verification. Still send them.
                        if verbose:
                            print(
                                f"Note: sig size {len(sig)} != expected {sig_bytes} "
                                f"for tcId={cur_tc_id}"
                            )

                    if external_mu:
                        mu = bytes.fromhex(test["mu"])
                    elif sig_interface == "external":
                        msg = bytes.fromhex(test["message"])
                        ctx = bytes.fromhex(test.get("context", ""))
                        hash_alg = test.get("hashAlg", "none")
                        mprime = build_mprime_external(pre_hash, ctx, msg, hash_alg)
                        mu = compute_mu_from_pk(pk, mprime)
                    elif sig_interface == "internal":
                        mprime = bytes.fromhex(test["message"])
                        mu = compute_mu_from_pk(pk, mprime)
                    else:
                        print(f"Skipping unsupported signatureInterface: {sig_interface}")
                        skipped += 1
                        continue

                    # Expected result: 0x00 = valid, 0x01 = invalid
                    expected_result = bytes([0x00]) if test_passed else bytes([0x01])
                    label = (
                        f"sigver tcId={cur_tc_id} ({sig_interface}/{pre_hash}) "
                        f"expect={'valid' if test_passed else 'invalid'}"
                    )

                    run_operation(
                        ledgerblue_cmd=ledgerblue_cmd,
                        ins=INS_MLDSA_VERIFY,
                        input_data=pk + sig + mu,
                        expected=expected_result,
                        label=label,
                        verbose=verbose,
                        param_set=param_set,
                    )

                else:
                    print(f"\nSkipping unsupported mode: {mode} (tcId={cur_tc_id})")
                    skipped += 1
                    continue

                passed += 1

            except Exception as exc:
                print(f"FAIL [{param_set}]: tcId={cur_tc_id} - {exc}", file=sys.stderr)
                failed += 1
                print(f"\nResults: {passed} passed, {failed} failed, {skipped} skipped (stopped on first error)")
                return 1

    if tc_id is not None and passed == 0 and failed == 0:
        if skipped > 0:
            print(f"SKIPPED: tcId={tc_id} skipped {skipped} tests from other parameter sets", file=sys.stderr)
        else:
            print(f"ERROR: tcId={tc_id} not found in {json_path} for {param_filter}", file=sys.stderr)
        return 1

    print(f"\nResults: {passed} passed, {failed} failed, {skipped} skipped")
    print_perf_summary()
    return 1 if failed else 0


def print_perf_summary() -> None:
    """Print a performance summary table grouped by parameter set and operation."""
    if not perf_records:
        return

    print("\n" + "=" * 115)
    print("PERFORMANCE SUMMARY")
    print("=" * 115)

    # Detailed per-test table
    hdr = (
        f"{'Param Set':<12} {'Operation':<10} {'Input (B)':>10} {'Output (B)':>11} "
        f"{'Total (s)':>10} {'Compute (s)':>12} {'Est. RAM (B)':>13}  Label"
    )
    print(hdr)
    print("-" * 115)
    for r in perf_records:
        print(
            f"{r.param_set:<12} {r.operation:<10} {r.input_bytes:>10,} {r.output_bytes:>11,} "
            f"{r.elapsed_s:>10.3f} {r.compute_s:>12.3f} {r.est_memory:>13,}  {r.label}"
        )

    # Aggregated summary by (param_set, operation)
    from collections import defaultdict
    groups: Dict[Tuple[str, str], List[PerfRecord]] = defaultdict(list)
    for r in perf_records:
        groups[(r.param_set, r.operation)].append(r)

    print("\n" + "-" * 115)
    print("AGGREGATED BY PARAMETER SET / OPERATION")
    print("-" * 115)
    agg_hdr = (
        f"{'Param Set':<12} {'Operation':<10} {'Count':>6} {'Input (B)':>10} {'Output (B)':>11} "
        f"{'Min (s)':>9} {'Avg (s)':>9} {'Max (s)':>9} {'Cpt Avg (s)':>12} {'Est. RAM (B)':>13}"
    )
    print(agg_hdr)
    print("-" * 115)

    for (ps, op) in sorted(groups.keys(), key=lambda k: (list(MLDSA_SIZES.keys()).index(k[0]) if k[0] in MLDSA_SIZES else 99, k[1])):
        recs = groups[(ps, op)]
        times = [r.elapsed_s for r in recs]
        ctimes = [r.compute_s for r in recs]
        count = len(recs)
        t_min = min(times)
        t_max = max(times)
        t_avg = sum(times) / count
        c_avg = sum(ctimes) / count
        # All records in the group share input/output sizes and memory estimate
        in_bytes = recs[0].input_bytes
        out_bytes = recs[0].output_bytes
        est_mem = recs[0].est_memory
        print(
            f"{ps:<12} {op:<10} {count:>6} {in_bytes:>10,} {out_bytes:>11,} "
            f"{t_min:>9.3f} {t_avg:>9.3f} {t_max:>9.3f} {c_avg:>12.3f} {est_mem:>13,}"
        )

    total_time = sum(r.elapsed_s for r in perf_records)
    total_compute = sum(r.compute_s for r in perf_records)
    print(f"\nTotal tests: {len(perf_records)}, Total time: {total_time:.3f}s, Total compute: {total_compute:.3f}s")
    print("=" * 115)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Send ML-DSA APDUs (keygen/sign/verify), fetch all response chunks, and verify expected output. "
            "Supports ML-DSA-44, ML-DSA-65, and ML-DSA-87 parameter sets."
        )
    )
    parser.add_argument(
        "--ledgerblue-cmd",
        default="python3 -m ledgerblue.runScript --apdu",
        help=(
            "Command that receives APDU bytes via stdin using syntax "
            "'echo <APDU_HEX> | <command>' (default: 'python3 -m ledgerblue.runScript --apdu')"
        ),
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Print APDU exchange progress")
    parser.add_argument(
        "-p", "--param-set",
        choices=["ML-DSA-44", "ML-DSA-65", "ML-DSA-87"],
        default="ML-DSA-44",
        help="ML-DSA parameter set (default: ML-DSA-44)",
    )

    subparsers = parser.add_subparsers(dest="operation", required=True)

    keygen_parser = subparsers.add_parser("keygen", help="Run ML-DSA key generation and compare pk||sk")
    keygen_parser.add_argument("seed", help="32-byte seed in hex (64 hex chars)")
    keygen_parser.add_argument("pk", help="Expected public key in hex (size depends on param set)")
    keygen_parser.add_argument("sk", help="Expected secret key in hex (size depends on param set)")

    sign_parser = subparsers.add_parser("sign", help="Run ML-DSA signature generation and compare sig")
    sign_parser.add_argument("sk", help="Secret key in hex (size depends on param set)")
    sign_parser.add_argument("msg", help="Message input in hex: raw M' by default, or mu with --mu")
    sign_parser.add_argument("sig", help="Expected signature in hex (size depends on param set)")
    sign_parser.add_argument(
        "--mu", action="store_true",
        help="Treat the sign input as a precomputed 64-byte mu instead of raw M'",
    )

    verify_parser = subparsers.add_parser("verify", help="Run ML-DSA signature verification")
    verify_parser.add_argument("pk", help="Public key in hex (size depends on param set)")
    verify_parser.add_argument("sig", help="Signature in hex (size depends on param set)")
    verify_parser.add_argument("msg", help="Message input in hex: raw M' by default, or mu with --mu")
    verify_parser.add_argument(
        "--mu", action="store_true",
        help="Treat the verify input as a precomputed 64-byte mu instead of raw M'",
    )
    verify_parser.add_argument(
        "--expect-valid", action="store_true", default=True,
        help="Expect signature to be valid (default)",
    )
    verify_parser.add_argument(
        "--expect-invalid", action="store_true",
        help="Expect signature to be invalid",
    )

    json_parser = subparsers.add_parser("json", help="Run ML-DSA test vectors from an ACVP JSON file")
    json_parser.add_argument("json_file", help="Path to ACVP JSON test vector file (keyGen, sigGen, or sigVer)")
    json_parser.add_argument(
        "--filter",
        choices=["ML-DSA-44", "ML-DSA-65", "ML-DSA-87"],
        help="Only run tests for this parameter set (default: run all)",
    )
    json_parser.add_argument(
        "--tc-id",
        type=int,
        help="Only run the test vector with this tcId",
    )

    args = parser.parse_args()

    try:
        ledgerblue_cmd = args.ledgerblue_cmd.strip()
        if not ledgerblue_cmd:
            raise ValueError("--ledgerblue-cmd must not be empty")

        param_set = args.param_set
        if param_set not in MLDSA_SIZES:
            raise ValueError(f"Unknown parameter set: {param_set}")

        seed_bytes, pk_bytes, sk_bytes, sig_bytes = MLDSA_SIZES[param_set]

        if args.operation == "keygen":
            seed = parse_hex_arg("seed", args.seed, seed_bytes)
            pk = parse_hex_arg("pk", args.pk, pk_bytes)
            sk = parse_hex_arg("sk", args.sk, sk_bytes)
            rc = run_operation(
                ledgerblue_cmd=ledgerblue_cmd,
                ins=INS_MLDSA_KEYGEN,
                input_data=seed,
                expected=pk + sk,
                label="pk||sk",
                verbose=args.verbose,
                param_set=param_set,
            )
            print_perf_summary()
            return rc

        if args.operation == "sign":
            sk = parse_hex_arg("sk", args.sk, sk_bytes)
            msg = bytes.fromhex(normalize_hex(args.msg))
            sig = parse_hex_arg("sig", args.sig, sig_bytes)
            mu = msg if args.mu else compute_mu_from_sk(sk, msg)
            if len(mu) != MLDSA_CRHBYTES:
                raise ValueError(f"mu must be exactly {MLDSA_CRHBYTES} bytes")
            rc = run_operation(
                ledgerblue_cmd=ledgerblue_cmd,
                ins=INS_MLDSA_SIGN,
                input_data=sk + mu,
                expected=sig,
                label="sig",
                verbose=args.verbose,
                param_set=param_set,
            )
            print_perf_summary()
            return rc

        if args.operation == "verify":
            pk = parse_hex_arg("pk", args.pk, pk_bytes)
            sig = parse_hex_arg("sig", args.sig, sig_bytes)
            msg = bytes.fromhex(normalize_hex(args.msg))
            mu = msg if args.mu else compute_mu_from_pk(pk, msg)
            if len(mu) != MLDSA_CRHBYTES:
                raise ValueError(f"mu must be exactly {MLDSA_CRHBYTES} bytes")
            expect_valid = not args.expect_invalid
            expected_result = bytes([0x00]) if expect_valid else bytes([0x01])
            rc = run_operation(
                ledgerblue_cmd=ledgerblue_cmd,
                ins=INS_MLDSA_VERIFY,
                input_data=pk + sig + mu,
                expected=expected_result,
                label=f"verify ({'valid' if expect_valid else 'invalid'})",
                verbose=args.verbose,
                param_set=param_set,
            )
            print_perf_summary()
            return rc

        if args.operation == "json":
            return run_json(
                ledgerblue_cmd=ledgerblue_cmd,
                json_path=args.json_file,
                verbose=args.verbose,
                param_filter=getattr(args, 'filter', None),
                tc_id=getattr(args, 'tc_id', None),
            )

        raise ValueError(f"Unsupported operation: {args.operation}")
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
