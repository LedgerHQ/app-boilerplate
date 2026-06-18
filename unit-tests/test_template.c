#include "unity.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_template(void) {
    TEST_ASSERT_EQUAL(1, 1);
}

int main(int argc, char *argv[]) {
    UNITY_BEGIN();

    RUN_TEST(test_template);

    return UNITY_END();
}
