/*****************************************************************************
 *   Ledger App NBGL_Tests.
 *   (c) 2020 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <string.h>   // memset, explicit_bzero

#include "os.h"

#include "nbgl_tests.h"
#include "../sw.h"
#include "../globals.h"
#include "../ui/display.h"
#include "../transaction/types.h"
#include "../transaction/deserialize.h"

int handler_test_transaction_review(uint8_t test_num, uint8_t sub_test_num) {
    // at the time-being, only 1 test
    if (test_num > 0) {
        return io_send_sw(SW_WRONG_P1P2);
    }
    // at the time-being, only 1 sub-test
    if (sub_test_num > 0) {
        return io_send_sw(SW_WRONG_P1P2);
    }

    return ui_display_transaction();
}

int handler_test_address_review(uint8_t test_num, uint8_t sub_test_num) {
    // at the time-being, only 1 test
    if (test_num > 0) {
        return io_send_sw(SW_WRONG_P1P2);
    }
    // at the time-being, only 1 sub-test
    if (sub_test_num > 0) {
        return io_send_sw(SW_WRONG_P1P2);
    }

    return ui_display_address();
}
