#pragma once

#include <stddef.h>   // size_t
#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool

/**
 * Maximum length of BIP32 path allowed.
 */
#define MAX_BIP32_PATH 10

/**
 * Function to read BIP32 path.
 *
 * @brief read BIP32 path in byte array.
 *
 * @param[in]  in pointer to byte array.
 * @param[in]  in_len length of byte array.
 * @param[out] out pointer to 32-bit integer array.
 * @param[in]  out_len lenght of the 32-bit integer array.
 *
 * @return true if success, false otherwise.
 *
 */
bool bip32_path_read(const uint8_t *in, size_t in_len, uint32_t *out, size_t out_len);

/**
 * Function to format BIP32 path as string.
 *
 * @brief convert 32 bit integer array with BIP32 path to string.
 *
 * @param[in]  bip32_path array of 32 bit integers.
 * @param[in]  bip32_path_len length of the array.
 * @param[out] out string representation of BIP32 path.
 * @param[in]  out_len length of the string.
 *
 * @return true if success, false otherwise.
 *
 */
bool bip32_path_format(const uint32_t *bip32_path,
                       size_t bip32_path_len,
                       char *out,
                       size_t out_len);
