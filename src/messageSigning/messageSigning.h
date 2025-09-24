#pragma once

#include "bip44.h"

int signRawMessageWithPath(const bip44_path_t* path,
                            const uint8_t* messageBuffer,
                            size_t messageSize,
                            uint8_t* outBuffer,
                            size_t outSize);

void getWitness(const bip44_path_t* path,
                const uint8_t* txHashBuffer,
                size_t txHashSize,
                uint8_t* outBuffer,
                size_t outSize);

void getCVoteRegistrationSignature(const bip44_path_t* path,
                                   const uint8_t* payloadHashBuffer,
                                   size_t payloadHashSize,
                                   uint8_t* outBuffer,
                                   size_t outSize);
