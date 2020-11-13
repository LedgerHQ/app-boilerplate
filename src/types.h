#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#include "common/bip32.h"

/**
 * Enumeration for the status of IO.
 *
 * @brief status of IO, either READY, RECEIVED or WAITING.
 *
 */
typedef enum {
    READY,     ///< ready for new event
    RECEIVED,  ///< data received
    WAITING    ///< waiting
} io_state_e;

/**
 * Enumeration with expected INS of APDU commands.
 *
 * @brief available INS for APDU command.
 *
 */
typedef enum {
    GET_VERSION = 0x03,    ///< version of the application
    GET_APP_NAME = 0x04,   ///< name of the application
    GET_PUBLIC_KEY = 0x05  ///< public key of corresponding BIP32 path
} command_e;

/**
 * Structure of APDU command to receive.
 *
 * @brief struct within APDU command fields (CLA, INS, P1, P2, Lc, Command data).
 *
 */
typedef struct {
    uint8_t cla;
    command_e ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc;
    uint8_t *data;
} command_t;
