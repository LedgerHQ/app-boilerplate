/**
 * @file nvram_data.h
 * @brief All definitions of the specific part of NVRAM structure for this application
 */
#pragma once

#include <stdint.h>

/**
 * @brief Oldest supported version of the NVRAM (for conversion)
 *
 */
#define NVRAM_FIRST_SUPPORTED_VERSION 1

/**
 * @brief Current version of the NVRAM structure
 * @note Set it to 1 to generate the first version of NVRAM data to fetch after a first launch.
 * Then set it back to 2 to generate the second version, then load fetched NVRAM after a
 * first launch.
 *
 */
#define NVRAM_STRUCT_VERSION 2

/**
 * @brief Definition of NVRAM properties for this App
 *
 */
#define NVRAM_DATA_PROPERTIES (CONTAINS_SETTINGS | CONTAINS_SENSITIVE_DATA)

/**
 * @brief Structure defining the NVRAM
 *
 */
typedef struct Nvram_data_s {
  uint8_t dummy1_allowed;
  uint8_t dummy2_allowed;
  uint8_t initialized;
#if (NVRAM_STRUCT_VERSION == 2)
  char string[30]; // added in v2
#endif
} Nvram_data_t;
