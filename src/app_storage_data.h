#pragma once

#include <stdint.h>

/**
 * @brief Oldest supported version of the application storage data structure
 * (for conversion).
 *
 */
#define APP_STORAGE_DATA_STRUCT_FIRST_SUPPORTED_VERSION 1

/**
 * @brief Current version of the application storage data structure
 * @note Set it to 1 to generate the first version of the data structure to
 * backup after a first launch.
 * Then set it back to 2 to generate the second version, then restore the
 * backed up data after a first launch.
 *
 */
#define APP_STORAGE_DATA_STRUCT_VERSION 2

/**
 * @brief Definition of the application storage properties of this App
 *
 */
#define APP_STORAGE_PROPERTIES (APP_STORAGE_PROP_SETTINGS | APP_STORAGE_PROP_DATA)

/**
 * @brief Structure defining the data stored in the application storage
 *
 */
typedef struct app_storage_data_s {
  uint8_t dummy1_allowed;
  uint8_t dummy2_allowed;
  uint8_t initialized;
#if (APP_STORAGE_DATA_STRUCT_VERSION == 2)
  char string[30]; // added in v2
#endif
} app_storage_data_t;
