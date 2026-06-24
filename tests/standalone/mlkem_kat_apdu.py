#!/usr/bin/env python3
"""
ML-KEM APDU Test Script

Tests ML-KEM key generation, encapsulation, and decapsulation over APDU
using the Ledger device. Supports ML-KEM-512, ML-KEM-768, and ML-KEM-1024
parameter sets.

APDU P2 encoding:
  - bits 0-1: parameter set (0=ML-KEM-512, 1=ML-KEM-768, 2=ML-KEM-1024)
  - bit 7: more flag (0=last chunk, 1=more chunks)
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
INS_MLKEM_KEYGEN = 0x30
INS_MLKEM_ENCAPSULATE = 0x31
INS_MLKEM_DECAPSULATE = 0x32
SW_SUCCESS = 0x9000
MAX_APDU_DATA_LEN = 255
MLKEM_SSBYTES = 32

# P2 encoding: bits 0-1 = param set, bit 7 = more flag
P2_MORE_FLAG = 0x80
P2_PARAM_512 = 0x00
P2_PARAM_768 = 0x01
P2_PARAM_1024 = 0x02

# Parameter set sizes: (pk_bytes, sk_bytes, ct_bytes)
MLKEM_SIZES: Dict[str, Tuple[int, int, int]] = {
    "ML-KEM-512": (800, 1632, 768),
    "ML-KEM-768": (1184, 2400, 1088),
    "ML-KEM-1024": (1568, 3168, 1568),
}

# Map parameter set name to P2 value
PARAM_SET_P2: Dict[str, int] = {
    "ML-KEM-512": P2_PARAM_512,
    "ML-KEM-768": P2_PARAM_768,
    "ML-KEM-1024": P2_PARAM_1024,
}

INS_TO_OP_NAME: Dict[int, str] = {
    INS_MLKEM_KEYGEN: "keygen",
    INS_MLKEM_ENCAPSULATE: "encap",
    INS_MLKEM_DECAPSULATE: "decap",
}


@dataclass
class Rapdu:
    data: bytes
    sw: int


@dataclass
class PerfRecord:
    """Stores timing and size information for a single ML-KEM operation."""
    param_set: str
    operation: str
    input_bytes: int
    output_bytes: int
    elapsed_s: float
    label: str
    compute_s: float = 0.0


perf_records: List[PerfRecord] = []


def normalize_hex(value: str) -> str:
    return re.sub(r"[^0-9a-fA-F]", "", value).upper()


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
    # Priority 1: explicit (data literal + trailing SW) pair anywhere in output.
    literal_with_sw = list(
        re.finditer(r"b['\"]([0-9A-Fa-f]*)['\"]\s*['\"]?([0-9A-Fa-f]{4})['\"]?", text, re.DOTALL)
    )
    if literal_with_sw:
        last = literal_with_sw[-1]
        data_hex = last.group(1)
        sw = int(last.group(2), 16)
        data = bytes.fromhex(data_hex) if data_hex else b""
        return Rapdu(data=data, sw=sw)

    # Priority 2: last bytes literal + standalone trailing SW (on its own line or at end).
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

        # Priority 3 (last resort): literal contains full RAPDU (data||SW).
        if len(data_hex) >= 4 and len(data_hex) % 2 == 0:
            sw_candidate = int(data_hex[-4:], 16)

            # Heuristic guard: APDU SW are typically 0x6xxx/0x9xxx or app-specific 0xBxxx/0xCxxx.
            # If unlikely, avoid mis-parsing payload tail as SW.
            if (sw_candidate & 0xF000) in (0x6000, 0x9000, 0xB000, 0xC000):
                data = bytes.fromhex(data_hex[:-4]) if len(data_hex) > 4 else b""
                return Rapdu(data=data, sw=sw_candidate)

    # Fallback: parse a plain RAPDU hex stream and split data||SW.
    # Keep only full bytes to avoid accidental single-char non-hex captures.
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
    param_set: str = "ML-KEM-512",
) -> int:
    """Execute an ML-KEM operation over APDU with chunked input/output."""
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
    # Start compute timer here: the device performs the ML-KEM operation
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
        # Response chunks: use same param set, no more flag
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
    record = PerfRecord(
        param_set=param_set,
        operation=op_name,
        input_bytes=len(input_data),
        output_bytes=total_len,
        elapsed_s=elapsed,
        label=label,
        compute_s=t_compute,
    )
    perf_records.append(record)

    print(f"KAT OK [{param_set}]: {label} response matches expected {label} (total {elapsed:.3f}s, compute {t_compute:.3f}s)")
    print(f"  total_len={total_len}, input_chunks={len(input_chunks)}, response_chunks={response_chunks}")
    return 0


def run_json(
    ledgerblue_cmd: str,
    json_path: str,
    verbose: bool,
    param_filter: str = None,
    tc_id: int = None,
) -> int:
    """Run ML-KEM test vectors from an ACVP JSON file.

    Args:
        ledgerblue_cmd: Command to send APDUs
        json_path: Path to ACVP JSON test vector file
        verbose: Print APDU exchange progress
        param_filter: If set, only run tests for this parameter set (e.g., "ML-KEM-768")
        tc_id: If set, only run the test with this tcId
    """
    with open(json_path) as f:
        data = json.load(f)

    mode = data.get("mode")
    passed = 0
    failed = 0
    skipped = 0

    for group in data.get("testGroups", []):
        param_set = group.get("parameterSet")
        if param_set not in MLKEM_SIZES:
            if verbose:
                print(f"Skipping unsupported parameter set: {param_set}")
            continue

        if param_filter and param_set != param_filter:
            continue

        pk_bytes, sk_bytes, ct_bytes = MLKEM_SIZES[param_set]
        func = group.get("function", "")

        for test in group.get("tests", []):
            cur_tc_id = test.get("tcId", "?")

            if tc_id is not None and cur_tc_id != tc_id:
                continue

            try:
                if mode == "keyGen":
                    d = bytes.fromhex(test["d"])
                    z = bytes.fromhex(test["z"])
                    ek = bytes.fromhex(test["ek"])
                    dk = bytes.fromhex(test["dk"])

                    if len(ek) != pk_bytes or len(dk) != sk_bytes:
                        raise ValueError(
                            f"Key size mismatch for {param_set}: "
                            f"ek={len(ek)} (expected {pk_bytes}), "
                            f"dk={len(dk)} (expected {sk_bytes})"
                        )

                    label = f"keygen tcId={cur_tc_id}"
                    run_operation(
                        ledgerblue_cmd=ledgerblue_cmd,
                        ins=INS_MLKEM_KEYGEN,
                        input_data=d + z,
                        expected=ek + dk,
                        label=label,
                        verbose=verbose,
                        param_set=param_set,
                    )

                elif mode == "encapDecap" and func == "encapsulation":
                    ek = bytes.fromhex(test["ek"])
                    m = bytes.fromhex(test["m"])
                    c = bytes.fromhex(test["c"])
                    k = bytes.fromhex(test["k"])

                    if len(ek) != pk_bytes or len(c) != ct_bytes:
                        raise ValueError(
                            f"Size mismatch for {param_set}: "
                            f"ek={len(ek)} (expected {pk_bytes}), "
                            f"c={len(c)} (expected {ct_bytes})"
                        )

                    label = f"encap tcId={cur_tc_id}"
                    run_operation(
                        ledgerblue_cmd=ledgerblue_cmd,
                        ins=INS_MLKEM_ENCAPSULATE,
                        input_data=ek + m,
                        expected=c + k,
                        label=label,
                        verbose=verbose,
                        param_set=param_set,
                    )

                elif mode == "encapDecap" and func == "decapsulation":
                    c = bytes.fromhex(test["c"])
                    dk = bytes.fromhex(test["dk"])
                    k = bytes.fromhex(test["k"])

                    if len(c) != ct_bytes or len(dk) != sk_bytes:
                        raise ValueError(
                            f"Size mismatch for {param_set}: "
                            f"c={len(c)} (expected {ct_bytes}), "
                            f"dk={len(dk)} (expected {sk_bytes})"
                        )

                    label = f"decap tcId={cur_tc_id}"
                    run_operation(
                        ledgerblue_cmd=ledgerblue_cmd,
                        ins=INS_MLKEM_DECAPSULATE,
                        input_data=c + dk,
                        expected=k,
                        label=label,
                        verbose=verbose,
                        param_set=param_set,
                    )

                else:
                    print(f"\nSkipping unsupported test function: {func} (tcId={cur_tc_id})")
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

    print("\n" + "=" * 105)
    print("PERFORMANCE SUMMARY")
    print("=" * 105)

    # Detailed per-test table
    hdr = (
        f"{'Param Set':<14} {'Operation':<8} {'Input (B)':>10} {'Output (B)':>11} "
        f"{'Total (s)':>10} {'Compute (s)':>12}  Label"
    )
    print(hdr)
    print("-" * 105)
    for r in perf_records:
        print(
            f"{r.param_set:<14} {r.operation:<8} {r.input_bytes:>10,} {r.output_bytes:>11,} "
            f"{r.elapsed_s:>10.3f} {r.compute_s:>12.3f}  {r.label}"
        )

    # Aggregated summary by (param_set, operation)
    from collections import defaultdict
    groups: Dict[Tuple[str, str], List[PerfRecord]] = defaultdict(list)
    for r in perf_records:
        groups[(r.param_set, r.operation)].append(r)

    print("\n" + "-" * 105)
    print("AGGREGATED BY PARAMETER SET / OPERATION")
    print("-" * 105)
    agg_hdr = (
        f"{'Param Set':<14} {'Operation':<8} {'Count':>6} {'Input (B)':>10} {'Output (B)':>11} "
        f"{'Min (s)':>9} {'Avg (s)':>9} {'Max (s)':>9} {'Cpt Avg (s)':>12}"
    )
    print(agg_hdr)
    print("-" * 105)

    for (ps, op) in sorted(groups.keys(), key=lambda k: (list(MLKEM_SIZES.keys()).index(k[0]) if k[0] in MLKEM_SIZES else 99, k[1])):
        recs = groups[(ps, op)]
        times = [r.elapsed_s for r in recs]
        ctimes = [r.compute_s for r in recs]
        count = len(recs)
        t_min = min(times)
        t_max = max(times)
        t_avg = sum(times) / count
        c_avg = sum(ctimes) / count
        in_bytes = recs[0].input_bytes
        out_bytes = recs[0].output_bytes
        print(
            f"{ps:<14} {op:<8} {count:>6} {in_bytes:>10,} {out_bytes:>11,} "
            f"{t_min:>9.3f} {t_avg:>9.3f} {t_max:>9.3f} {c_avg:>12.3f}"
        )

    total_time = sum(r.elapsed_s for r in perf_records)
    total_compute = sum(r.compute_s for r in perf_records)
    print(f"\nTotal tests: {len(perf_records)}, Total time: {total_time:.3f}s, Total compute: {total_compute:.3f}s")
    print("=" * 105)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Send ML-KEM APDUs (keygen/encap/decap), fetch all response chunks, and verify expected output. "
            "Supports ML-KEM-512, ML-KEM-768, and ML-KEM-1024 parameter sets."
        )
    )
    parser.add_argument(
        "--ledgerblue-cmd",
        default="python3 -m ledgerblue.runScript --apdu",
        help=(
            "Command that receives APDU bytes via stdin using syntax "
            "'echo <APDU_HEX> | <command>' (default: 'python3 ledgerblue.runScript --apdu')"
        ),
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Print APDU exchange progress")
    parser.add_argument(
        "-p", "--param-set",
        choices=["ML-KEM-512", "ML-KEM-768", "ML-KEM-1024"],
        default="ML-KEM-512",
        help="ML-KEM parameter set (default: ML-KEM-512)",
    )

    subparsers = parser.add_subparsers(dest="operation", required=True)

    keygen_parser = subparsers.add_parser("keygen", help="Run ML-KEM key generation and compare ek||dk")
    keygen_parser.add_argument("dz", help="64-byte d||z in hex (128 hex chars)")
    keygen_parser.add_argument("ek", help="Encapsulation/public key in hex (size depends on param set)")
    keygen_parser.add_argument("dk", help="Decapsulation/secret key in hex (size depends on param set)")

    encap_parser = subparsers.add_parser("encap", help="Run ML-KEM encapsulation and compare c||k")
    encap_parser.add_argument("ek", help="Encapsulation/public key in hex (size depends on param set)")
    encap_parser.add_argument("m", help="32-byte encapsulation randomness/message in hex")
    encap_parser.add_argument("c", help="Expected ciphertext in hex (size depends on param set)")
    encap_parser.add_argument("k", help="32-byte expected shared secret in hex")

    decap_parser = subparsers.add_parser("decap", help="Run ML-KEM decapsulation and compare k")
    decap_parser.add_argument("c", help="Ciphertext in hex (size depends on param set)")
    decap_parser.add_argument("dk", help="Decapsulation/secret key in hex (size depends on param set)")
    decap_parser.add_argument("k", help="32-byte expected shared secret in hex")

    json_parser = subparsers.add_parser("json", help="Run ML-KEM test vectors from an ACVP JSON file")
    json_parser.add_argument("json_file", help="Path to ACVP JSON test vector file (keyGen or encapDecap)")
    json_parser.add_argument(
        "--filter",
        choices=["ML-KEM-512", "ML-KEM-768", "ML-KEM-1024"],
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
        if param_set not in MLKEM_SIZES:
            raise ValueError(f"Unknown parameter set: {param_set}")

        pk_bytes, sk_bytes, ct_bytes = MLKEM_SIZES[param_set]

        if args.operation == "keygen":
            dz = parse_hex_arg("d||z", args.dz, 2 * MLKEM_SSBYTES)
            ek = parse_hex_arg("ek", args.ek, pk_bytes)
            dk = parse_hex_arg("dk", args.dk, sk_bytes)
            rc = run_operation(
                ledgerblue_cmd=ledgerblue_cmd,
                ins=INS_MLKEM_KEYGEN,
                input_data=dz,
                expected=ek + dk,
                label="ek||dk",
                verbose=args.verbose,
                param_set=param_set,
            )
            print_perf_summary()
            return rc

        if args.operation == "encap":
            ek = parse_hex_arg("ek", args.ek, pk_bytes)
            m = parse_hex_arg("m", args.m, MLKEM_SSBYTES)
            c = parse_hex_arg("c", args.c, ct_bytes)
            k = parse_hex_arg("k", args.k, MLKEM_SSBYTES)
            rc = run_operation(
                ledgerblue_cmd=ledgerblue_cmd,
                ins=INS_MLKEM_ENCAPSULATE,
                input_data=ek + m,
                expected=c + k,
                label="c||k",
                verbose=args.verbose,
                param_set=param_set,
            )
            print_perf_summary()
            return rc

        if args.operation == "decap":
            c = parse_hex_arg("c", args.c, ct_bytes)
            dk = parse_hex_arg("dk", args.dk, sk_bytes)
            k = parse_hex_arg("k", args.k, MLKEM_SSBYTES)
            rc = run_operation(
                ledgerblue_cmd=ledgerblue_cmd,
                ins=INS_MLKEM_DECAPSULATE,
                input_data=c + dk,
                expected=k,
                label="k",
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
