#pragma once

#include "buffer.h"

// TODO comments

void handler_sign_opcert(buffer_t *cdata);

/**
 * Finalize operational certificate signing after user confirmation/rejection.
 *
 * If confirmed, assembles the opcert bytestring and signs it with the pool cold key.
 * Sets appropriate state and sends response back to client.
 *
 * @param confirmed Whether the user confirmed or rejected the operation
 */
void finalize_sign_opcert(bool confirmed);
