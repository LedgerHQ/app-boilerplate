# converts entries from Cardano Token Registry (json) into code in C used in the app

import json
import hashlib

# WARNING --- make sure that:
#     1. token tickers are meaningful and none is "(unknown decimals)"
#     2. buffers (e.g. tokenAmountStr) are big enough to hold the tickers
filename = "tokenList.json"

registry = json.load(open(filename))

def formatHexByte(b):
	return f"0x{b:02x}"

def bytestringToC(bstr):
	return "{ " + ", ".join([formatHexByte(b) for b in bstr]) + " }"

def tokenLine(tokenEntry):
	subject = bytes.fromhex(tokenEntry["assetSubject"])
	fingerprint = hashlib.blake2b(subject, digest_size=20).digest()

	policyId = subject[0:28]
	assetName = subject[28:]
	#print(f"{policyId.hex()},   {assetName.hex()},   {fingerprint.hex()},   {bytestringToC(policyId)},   {bytestringToC(assetName)}")

	if "ticker" in tokenEntry and len(tokenEntry["ticker"]) > 0:
		ticker = tokenEntry["ticker"]
	else:
		ticker = tokenEntry["name"]

	line = "{ "
	line += bytestringToC(fingerprint)
	line += ", "
	line += str(tokenEntry["decimals"])
	line += ", "
	line += '"' + ticker + '"'
	line += " }"
	return line

allLines = ",\n".join([tokenLine(t) for t in registry])

outputFile = open('token_data.csource', 'w')
outputFile.write(allLines)
outputFile.write('\n')
