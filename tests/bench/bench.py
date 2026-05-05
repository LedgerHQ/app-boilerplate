import signal
import struct
import time
from enum import IntEnum

import sympy
from ledgercomm import Transport

# speculos
trans = Transport(interface="tcp", server="127.0.0.1", port=9999, debug=False)
# physical device
#trans = Transport(interface="hid", debug=False)

class Ins(IntEnum):
    BENCH_PRIME = 0x01
    BENCH_FIBO = 0x02
    BENCH_BW = 0x03


class BwType(IntEnum):
    IN = 0x00
    OUT = 0x01
    BIDIR = 0x02


def bench_prime() -> None:
    times = []
    max_count = 100000
    ref_value = sympy.prime(max_count)

    for _ in range(5):
        before = time.time_ns()
        trans.send(cla=0xe0, ins=Ins.BENCH_PRIME, p1=0x00, p2=0x00, cdata=struct.pack(">I", 100000))
        _, res = trans.recv()
        value = struct.unpack(">I", res)[0]
        assert value == ref_value
        after = time.time_ns()
        times.append(after - before)
        print("[%u] %u" % (len(times) - 1, times[-1]))

    avg = sum(times) // len(times)
    print("=> avg of %u ns" % (avg))

def bench_bw(bw_type: BwType, end: int) -> None:
    sent = 0
    recvd = 0
    step = 0
    delay = 1 # seconds
    run = True

    def update(_signum: int, _frame) -> None:
        nonlocal sent
        nonlocal step
        nonlocal delay
        nonlocal end
        nonlocal run

        step += 1
        elapsed = step * delay # seconds
        print("IN : %s kB/s" % ((sent // elapsed) / 1000))
        print("OUT: %s kB/s" % ((recvd // elapsed) / 1000))

        if elapsed >= end:
            run = False

    print("Running %s for %u seconds" % (bw_type.name, end))
    buf = bytes(255) if bw_type in [BwType.IN, BwType.BIDIR] else b""
    signal.signal(signal.SIGALRM, update)
    signal.setitimer(signal.ITIMER_REAL, delay, delay)
    while run:
        trans.send(cla=0xe0, ins=Ins.BENCH_BW, p1=bw_type, p2=0x00, cdata=buf)
        _, res = trans.recv()
        sent += len(buf)
        recvd += len(res)

#bench_prime()
bench_bw(BwType.IN, 10)
bench_bw(BwType.OUT, 10)
bench_bw(BwType.BIDIR, 10)
