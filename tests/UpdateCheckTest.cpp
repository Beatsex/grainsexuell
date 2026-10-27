#include "UpdateCheck.h"
#include <catch2/catch_test_macros.hpp>

using grain::update::isNewer;

TEST_CASE("update: a release tag is newer only when its version number is higher")
{
    REQUIRE(isNewer("v0.3.0", "0.2.0"));
    REQUIRE(isNewer("0.2.1", "0.2.0"));
    REQUIRE(isNewer("v1.0", "0.9.9"));
    REQUIRE(isNewer("v0.10.0", "0.9.0")); // numbers, not text
    REQUIRE(isNewer("v0.4.0-beta", "0.3.0"));
    REQUIRE_FALSE(isNewer("v0.3.0", "0.3.0"));
    REQUIRE_FALSE(isNewer("v0.2.9", "0.3.0"));
    REQUIRE_FALSE(isNewer("", "0.3.0"));
    REQUIRE_FALSE(isNewer("latest", "0.3.0"));
    REQUIRE(std::string(GRAIN_VERSION) == "0.3.0");
}
