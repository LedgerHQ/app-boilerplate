#ifndef _TYPES_H_
#define _TYPES_H_

#include <stddef.h>
#include <stdint.h>

/**
 * Enumeration for the status of IO.
 *
 * @brief status of IO.
 *
 */
typedef enum {
    READY,     ///< ready for new event
    RECEIVED,  ///< data received
    WAITING    ///< waiting
} io_state_e;

/**
 * Structure of APDU response data to send.
 *
 * @brief response buffer with data and data length of APDU response.
 *
 */
typedef struct {
    uint8_t *data;    ///< pointer to buffer with response data
    size_t data_len;  ///< length of response data
} response_t;

/**
 * Enumeration with expected INS of APDU commands.
 *
 * @brief available INS for APDU command.
 *
 */
typedef enum {
    GET_VERSION = 0x03,   ///< version of the application
    GET_APP_NAME = 0x04,  ///< name of the application
} command_e;

/**
 * Structure of APDU command to receive.
 *
 * @brief struct of APDU command fields (CLA, INS, P1, P2, Lc, Command data).
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

#endif  // _TYPES_H_
