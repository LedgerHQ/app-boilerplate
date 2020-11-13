#pragma once

#include <stddef.h>   // size_t
#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool

#include "buffer.h"

/**
 * Maximum length of BIP32 path allowed.
 */
#define MAX_BIP32_PATH 10

/**
 * Function to convert BIP32 path bytes to array of integers.
 *
 * @brief read bytes of BIP32 path in buffer and convert to array of 32 bit integers.
 *
 * @param[in,out] buf buffer with bytes of BIP32 path.
 * @param[out]    out pointer to array of 32 bit integers.
 * @param[in]     out_len lenght of the array pointed.
 *
 * @return true if success, false otherwise.
 *
 */
bool bip32_path_from_buffer(buffer_t *buf, uint32_t *out, size_t out_len);

/**
 * Function to convert BIP32 path to string.
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
bool bip32_path_to_str(const uint32_t *bip32_path,
                       size_t bip32_path_len,
                       char *out,
                       size_t out_len);
