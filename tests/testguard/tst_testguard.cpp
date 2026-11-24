// Test build allowlist (app/backend/testguard), the logic behind NvHTTP::isAllowedTestAddress:
// loopback only, ports 40000-44999 only.

#include "../../app/backend/testguard.h"

#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}
}

int main()
{
    check(TestGuard::isAllowedAddress("127.0.0.1", 40989), "loopbackFirstTestHost");
    check(!TestGuard::isAllowedAddress("127.0.0.1", 47989), "defaultPortRefused");
    check(!TestGuard::isAllowedAddress("127.0.0.1", 45989), "realHostOfThisPcRefused");
    check(!TestGuard::isAllowedAddress("192.168.1.231", 40989), "lanAddressRefused");
    check(TestGuard::isAllowedAddress("localhost", 41984), "localhostAllowed");
    check(!TestGuard::isAllowedAddress("127.0.0.1", 39999), "belowRangeRefused");
    check(!TestGuard::isAllowedAddress("127.0.0.1", 45000), "aboveRangeRefused");
    check(TestGuard::isAllowedAddress("127.0.0.1", 40000), "lowerBoundAllowed");
    check(TestGuard::isAllowedAddress("127.0.0.1", 44999), "upperBoundAllowed");
    check(TestGuard::isAllowedAddress("::1", 42000), "ipv6LoopbackAllowed");
    check(TestGuard::isAllowedAddress("[::1]", 42000), "bracketedIpv6Allowed");
    check(!TestGuard::isAllowedAddress("", 42000), "emptyHostRefused");
    check(!TestGuard::isAllowedAddress("10.0.0.1", 42000), "otherPrivateRefused");
    if (failures == 0) {
        std::printf("tst_testguard: all passed\n");
    }
    return failures == 0 ? 0 : 1;
}
