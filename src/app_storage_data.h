#pragma once

#include <stdint.h>

/**
 * @brief Oldest supported version of the application storage data structure
 * (for conversion). v1 is not anymore supported
 *
 */
#define APP_STORAGE_DATA_STRUCT_FIRST_SUPPORTED_VERSION 2

/**
 * @brief Current version of the application storage data structure
 * @note Set it to 1 to generate the first version of the data structure to
 * backup after a first launch.
 * Then set it back to 2 to generate the second version, then restore the
 * backed up data after a first launch.
 *
 */
#define APP_STORAGE_DATA_STRUCT_VERSION 3

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
    uint32_t version;
    uint8_t dummy1_allowed;
    uint8_t dummy2_allowed;
#if (APP_STORAGE_DATA_STRUCT_VERSION == 3)
    char string[30];  // added in v3
#endif
} app_storage_data_t;
_Static_assert(sizeof(app_storage_data_t) <= APP_STORAGE_SIZE, "The application storage size requested in Makefile is not sufficient");

/* RAM representation of the app data storage */
extern app_storage_data_t sd_cache;

/**
 * App storage accessors
 */
#define APP_STORAGE_WRITE_ALL(src_buf) app_storage_pwrite(src_buf, sizeof(app_storage_data_t), 0)

#define APP_STORAGE_WRITE_F(field, src_buf)                       \
    app_storage_pwrite(src_buf,                                   \
                       sizeof(((app_storage_data_t *) 0)->field), \
                       offsetof(app_storage_data_t, field))

#define APP_STORAGE_READ_ALL(dst_buf) app_storage_pread(dst_buf, sizeof(app_storage_data_t), 0)

#define APP_STORAGE_READ_F(field, dst_buf)                       \
    app_storage_pread(dst_buf,                                   \
                      sizeof(((app_storage_data_t *) 0)->field), \
                      offsetof(app_storage_data_t, field))
