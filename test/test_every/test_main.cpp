#include <unity.h>
#include "Every.h"

void setUp() {}
void tearDown() {}

void test_due_once_per_period() {
  Every e(100);
  TEST_ASSERT_TRUE(e.due(0));
  TEST_ASSERT_FALSE(e.due(50));
  TEST_ASSERT_TRUE(e.due(100));
  TEST_ASSERT_FALSE(e.due(150));
}

void test_due_survives_millis_wrap() {
  Every w(10);
  TEST_ASSERT_TRUE(w.due(0xFFFFFFF8u));
  TEST_ASSERT_FALSE(w.due(0xFFFFFFFCu));
  TEST_ASSERT_TRUE(w.due(2));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_due_once_per_period);
  RUN_TEST(test_due_survives_millis_wrap);
  return UNITY_END();
}
