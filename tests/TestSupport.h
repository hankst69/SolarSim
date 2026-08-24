#pragma once

// Minimal header only assertion helpers for the geolib tests.
// Keeps the test tree dependency free, matching the library itself.

#include <cmath>
#include <cstdio>
#include <string>

namespace geotest {

inline int& failureCount()
{
    static int failures = 0;
    return failures;
}

inline void reportFailure(const char* file, int line, const std::string& message)
{
    std::printf("FAILED %s:%d\n  %s\n", file, line, message.c_str());
    ++failureCount();
}

inline void checkTrue(bool value, const char* expression, const char* file, int line)
{
    if (!value) {
        reportFailure(file, line, std::string("expected true: ") + expression);
    }
}

inline void checkNear(double actual, double expected, double tolerance, const char* expression,
                      const char* file, int line)
{
    if (std::isnan(actual) || std::fabs(actual - expected) > tolerance) {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%s: expected %.9f +/- %g, got %.9f", expression,
                      expected, tolerance, actual);
        reportFailure(file, line, buffer);
    }
}

inline void checkEqualInt(long long actual, long long expected, const char* expression,
                          const char* file, int line)
{
    if (actual != expected) {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%s: expected %lld, got %lld", expression, expected,
                      actual);
        reportFailure(file, line, buffer);
    }
}

inline void checkEqualString(const std::string& actual, const std::string& expected,
                             const char* expression, const char* file, int line)
{
    if (actual != expected) {
        reportFailure(file, line, std::string(expression) + ": expected \"" + expected +
                                      "\", got \"" + actual + "\"");
    }
}

inline void checkEqualStringArgs(const char* file, int line, const std::string& actual,
                                 const std::string& expected)
{
    checkEqualString(actual, expected, "string", file, line);
}

/// Prints the result and returns the process exit code.
inline int summarize(const char* suiteName)
{
    if (failureCount() == 0) {
        std::printf("PASSED %s\n", suiteName);
        return 0;
    }
    std::printf("FAILED %s (%d failure(s))\n", suiteName, failureCount());
    return 1;
}

} // namespace geotest

#define CHECK_TRUE(...) ::geotest::checkTrue((__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)
#define CHECK_FALSE(...) ::geotest::checkTrue(!(__VA_ARGS__), "!" #__VA_ARGS__, __FILE__, __LINE__)
#define CHECK_NEAR(actual, expected, tol) \
    ::geotest::checkNear((actual), (expected), (tol), #actual, __FILE__, __LINE__)
#define CHECK_EQ_INT(actual, expected) \
    ::geotest::checkEqualInt((actual), (expected), #actual, __FILE__, __LINE__)
#define CHECK_EQ_STR(...) ::geotest::checkEqualStringArgs(__FILE__, __LINE__, __VA_ARGS__)
