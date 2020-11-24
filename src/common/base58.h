#pragma once

#include <stddef.h>   // size_t
#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool

/**
 * Maximum length of input when decoding in base 58.
 */
#define MAX_DEC_INPUT_SIZE 164
/**
 * Maximum length of input when encoding in base 58.
 */
#define MAX_ENC_INPUT_SIZE 120

/**
 * Function to decode in base 58.
 *
 * @brief decode string to bytes in base 58.
 *
 * @see https://tools.ietf.org/html/draft-msporny-base58-02
 *
 * @param[in]  in pointer to input string buffer to be decoded.
 * @param[in]  in_len length of the string buffer.
 * @param[out] out pointer to array.
 * @param[in]  out_len maximum length to be stored in out.
 *
 */
int base58_decode(const char *in, size_t in_len, uint8_t *out, size_t out_len);

/**
 * Function to encode in base 58.
 *
 * @brief encode bytes to string in base 58.
 *
 * @see https://tools.ietf.org/html/draft-msporny-base58-02
 *
 * @param[in]  in pointer to input byte buffer to be encoded.
 * @param[in]  in_len length of the byte buffer.
 * @param[out] out pointer to output string buffer.
 * @param[in]  out_len maximum length to be stored in out.
 *
 */
int base58_encode(const uint8_t *in, size_t in_len, char *out, size_t out_len);
