import struct
import time

from ledgercomm import Transport
import sympy

# trans = Transport(interface="tcp", server="127.0.0.1", port=9999, debug=False)
trans = Transport(interface="hid", debug=False)

times = []
max_count = 100000
ref_value = sympy.prime(max_count)

for _ in range(5):
    before = time.time_ns()
    trans.send(cla=0xe0, ins=0x07, p1=0x00, p2=0x00, cdata=struct.pack(">I", max_count))
    sw, res = trans.recv()
    value = struct.unpack(">I", res)[0]
    assert value == ref_value
    after = time.time_ns()
    times.append(after - before)
    print("[%u] %u" % (len(times) - 1, times[-1]))

avg = sum(times) // len(times)
print("=> avg of %u ns" % (avg))
