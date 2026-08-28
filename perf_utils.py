from ledgerblue import comm
from ledgerblue.hexLoader import HexLoader

from ledgerblue.comm import Dongle, getDongle
from ledgerblue.deployed import getDeployedSecretV2
from ledgerblue.ecWrapper import PrivateKey, PublicKey
from ledgerblue.hexLoader import HexLoader

# PAYLOAD_SIZE = 255  # Max APDU data payload

# Connect to hardware with SCP

# Key used for SCP and endorsement certificate computing
HSM_ATTESTATION_KEY =  "0000"

USE_SCP = False

if USE_SCP:
    PAYLOAD_SIZE = 255 - 17  # Max APDU data payload
else:
    PAYLOAD_SIZE = 255  # Max APDU data payload

# Total data size to send (can be larger than APDU max payload)
TOTAL_DATA_SIZE = 100000

if __name__ == "__main__":
    # Connect to hardware
    dongle = comm.getDongle()

    if USE_SCP:
        target_id = int(0x33400004)
        # "set" and "reset" command need authentication
        secret = getDeployedSecretV2(dongle, bytearray.fromhex(HSM_ATTESTATION_KEY), target_id)

        loader = HexLoader(dongle, 0xE0, True, secret)

        # Send data in chunks of at most PAYLOAD_SIZE bytes
        remaining = TOTAL_DATA_SIZE
        cnt = 0
        while remaining > 0:
            chunk_len = min(PAYLOAD_SIZE, remaining)
            resp = loader.exchange(0xE0, 0x00, 0x00, 0x00, bytes([0xFF] + [0xA5] * chunk_len))
            remaining -= chunk_len
            cnt += 1
    else:
        loader = HexLoader(dongle)

        # Send data in chunks of at most PAYLOAD_SIZE bytes
        remaining = TOTAL_DATA_SIZE
        cnt = 0
        while remaining > 0:
            chunk_len = min(PAYLOAD_SIZE, remaining)
            try:
                resp = loader.exchange(0xE0, 0xFF, 0x00, 0x00, bytes([0xA5] * chunk_len))
            except Exception as e:
                pass
            remaining -= chunk_len
            cnt += 1
