#pragma once

#include <stdint.h>

/**
 * Instruction class of the Boilerplate application.
 */
#define CLA 0xE0

/**
 * Length of APPNAME variable in Makefile.
 */
#define APPNAME_LEN (sizeof(APPNAME) - 1)

/**
 * Length of (MAJOR_VERSION || MINOR_VERSION || PATCH_VERSION) variables in Makefile.
 */
#define APPVERSION_LEN 3

/**
 * Maximum length of application name (APPNAME variable in Makefile).
 */
#define MAX_APPNAME_LEN 64

/**
 * Maximum transaction length in bytes.
 */
#define MAX_TRANSACTION_LEN 510

/**
 * Maximum signature length in bytes.
 */
#define MAX_DER_SIG_LEN 72
