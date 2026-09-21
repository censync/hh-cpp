#include "test_framework.hpp"

extern "C" int hh_test_c_api(void);

TEST_CASE("c api: the checks of the C translation unit") {
    EXPECT_EQ(hh_test_c_api(), 0);
}
