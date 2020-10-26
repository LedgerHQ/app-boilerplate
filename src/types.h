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
 * Structure for buffering APDU command or response.
 *
 * @brief buffer for bytes of APDU.
 *
 */
typedef struct {
    uint8_t *bytes;  ///< pointer to buffer of bytes
    size_t size;     ///< size of pointed bytes buffer
} buf_t;

/**
 * Enumeration with expected INS of APDU commands.
 *
 * @brief available INS for APDU command.
 *
 */
typedef enum {
    GET_VERSION = 0x03,   ///< version of the application command
    GET_APP_NAME = 0x04,  ///< name of the application command
} cmd_e;

#endif  // _TYPES_H_
