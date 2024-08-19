#pragma once

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool

/**
 * Handler for @ref TEST_REVIEW1 command. Executes the test
 *
 *
 * @param[in]     test_num
 *   Index of the test.
 * @param[in]     sub_test_num
 *   Index of the sub-test.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
int handler_test_transaction_review(uint8_t test_num, uint8_t sub_test_num);

/**
 * Handler for @ref TEST_REVIEW2 command. Executes the test
 *
 *
 * @param[in]     test_num
 *   Index of the test.
 * @param[in]     sub_test_num
 *   Index of the sub-test.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
int handler_test_address_review(uint8_t test_num, uint8_t sub_test_num);
