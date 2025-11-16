#pragma once

#include "addressUtils/bip44.h"

size_t deriveAddress_byron(const bip44_path_t* pathSpec,
                           uint32_t protocolMagic,
                           uint8_t* outBuffer,
                           size_t outSize);

// Note: validates the overall address structure at the same time
// Returns true on success, false if address is invalid
// out_protocol_magic is set to the protocol magic value on success
bool extractProtocolMagic(const uint8_t* addressBuffer, size_t addressSize, uint32_t* out_protocol_magic);
