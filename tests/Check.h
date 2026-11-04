#pragma once
#include <iostream>
inline int checkFailures = 0;
inline bool check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        ++checkFailures;
        std::cerr << file << ':' << line << ": expected " << expression << " got false\n";
    }
    return condition;
}
template <class A, class B>
void checkEqual(const A& a, const B& b, const char* file, int line) {
    if (!(a == b)) {
        ++checkFailures;
        std::cerr << file << ':' << line << ": expected " << b << " got " << a << '\n';
    }
}
inline int checkResult() {
    return checkFailures == 0 ? 0 : 1;
}
#define CHECK(cond) check(static_cast<bool>(cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) checkEqual((a), (b), __FILE__, __LINE__)
#define REQUIRE(cond)                                                                                                                      \
    do {                                                                                                                                   \
        if (!CHECK(cond)) {                                                                                                                \
            return checkResult();                                                                                                          \
        }                                                                                                                                  \
    } while (false)
