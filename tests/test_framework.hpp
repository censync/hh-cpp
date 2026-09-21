#pragma once

// Tiny self-contained test framework with no external dependency.
// Each TEST_CASE registers itself through a static initialiser; main() runs
// every registered case and exits non-zero if any expectation failed.

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace hh_test {

struct case_failure {
    std::string file;
    int line;
    std::string detail;
};

struct test_case {
    const char* name;
    void (*fn)(std::vector<case_failure>&);
};

inline std::vector<test_case>& registry() {
    static std::vector<test_case> r;
    return r;
}

struct registrar {
    registrar(const char* name, void (*fn)(std::vector<case_failure>&)) {
        registry().push_back({name, fn});
    }
};

inline int run_all() {
    std::size_t passed = 0;
    std::size_t failed = 0;
    for (const auto& c : registry()) {
        std::vector<case_failure> failures;
        try {
            c.fn(failures);
        } catch (const std::exception& e) {
            failures.push_back({"<unhandled exception>", 0, e.what()});
        } catch (...) {
            failures.push_back({"<unhandled exception>", 0, "<non-std exception>"});
        }
        if (failures.empty()) {
            std::cout << "[ OK ] " << c.name << "\n";
            ++passed;
        } else {
            std::cout << "[FAIL] " << c.name << "\n";
            for (const auto& f : failures) {
                std::cout << "       " << f.file << ":" << f.line << ": " << f.detail << "\n";
            }
            ++failed;
        }
    }
    std::cout << "\n"
              << passed << " passed, " << failed << " failed, " << registry().size() << " total\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace hh_test

#define HH_TEST_CONCAT_INNER(a, b) a##b
#define HH_TEST_CONCAT(a, b) HH_TEST_CONCAT_INNER(a, b)

#define TEST_CASE(name_literal)                                                               \
    static void HH_TEST_CONCAT(hh_test_fn_,                                                   \
                               __LINE__)(std::vector<::hh_test::case_failure> & hh_failures); \
    static ::hh_test::registrar HH_TEST_CONCAT(hh_test_reg_, __LINE__){                       \
        name_literal, &HH_TEST_CONCAT(hh_test_fn_, __LINE__)};                                \
    static void HH_TEST_CONCAT(hh_test_fn_,                                                   \
                               __LINE__)(std::vector<::hh_test::case_failure> & hh_failures)

#define EXPECT_TRUE(cond)                                                                \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            hh_failures.push_back(                                                       \
                {__FILE__, __LINE__, std::string{"EXPECT_TRUE("} + #cond + ") failed"}); \
        }                                                                                \
    } while (0)

#define EXPECT_FALSE(cond)                                                                \
    do {                                                                                  \
        if ((cond)) {                                                                     \
            hh_failures.push_back(                                                        \
                {__FILE__, __LINE__, std::string{"EXPECT_FALSE("} + #cond + ") failed"}); \
        }                                                                                 \
    } while (0)

#define EXPECT_EQ(a, b)                                                                    \
    do {                                                                                   \
        auto&& hh_a = (a);                                                                 \
        auto&& hh_b = (b);                                                                 \
        if (!(hh_a == hh_b)) {                                                             \
            std::ostringstream hh_oss;                                                     \
            hh_oss << "EXPECT_EQ(" << #a << ", " << #b << "): " << hh_a << " != " << hh_b; \
            hh_failures.push_back({__FILE__, __LINE__, hh_oss.str()});                     \
        }                                                                                  \
    } while (0)

#define EXPECT_NE(a, b)                                                           \
    do {                                                                          \
        auto&& hh_a = (a);                                                        \
        auto&& hh_b = (b);                                                        \
        if (hh_a == hh_b) {                                                       \
            std::ostringstream hh_oss;                                            \
            hh_oss << "EXPECT_NE(" << #a << ", " << #b << "): both are " << hh_a; \
            hh_failures.push_back({__FILE__, __LINE__, hh_oss.str()});            \
        }                                                                         \
    } while (0)
