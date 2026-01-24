#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <sstream>

// Simple test framework for C++20

class TestCase {
public:
    std::string name;
    std::function<bool()> test_func;
    
    TestCase(const std::string& n, std::function<bool()> f) : name(n), test_func(f) {}
};

class TestSuite {
private:
    std::string suite_name;
    std::vector<TestCase> tests;
    int passed = 0;
    int failed = 0;
    
public:
    TestSuite(const std::string& name) : suite_name(name) {}
    
    void addTest(const std::string& name, std::function<bool()> test_func) {
        tests.emplace_back(name, test_func);
    }
    
    void run() {
        std::cout << "\n=== " << suite_name << " ===" << std::endl;
        for (auto& test : tests) {
            std::cout << "  " << test.name << ": ";
            try {
                if (test.test_func()) {
                    std::cout << "PASSED" << std::endl;
                    passed++;
                } else {
                    std::cout << "FAILED" << std::endl;
                    failed++;
                }
            } catch (const std::exception& e) {
                std::cout << "EXCEPTION: " << e.what() << std::endl;
                failed++;
            }
        }
    }
    
    int getPassed() const { return passed; }
    int getFailed() const { return failed; }
    int getTotal() const { return tests.size(); }
};

class TestRunner {
private:
    std::vector<TestSuite*> suites;
    
public:
    void addSuite(TestSuite* suite) {
        suites.push_back(suite);
    }
    
    int runAll() {
        int total_passed = 0;
        int total_failed = 0;
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "       HISTORIC RALLY METER TESTS" << std::endl;
        std::cout << "========================================" << std::endl;
        
        for (auto* suite : suites) {
            suite->run();
            total_passed += suite->getPassed();
            total_failed += suite->getFailed();
        }
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "SUMMARY: " << total_passed << " passed, " 
                  << total_failed << " failed, "
                  << (total_passed + total_failed) << " total" << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        return total_failed;
    }
};

// Assertion macros
#define ASSERT_TRUE(expr) \
    if (!(expr)) { std::cerr << "  ASSERT_TRUE failed: " << #expr << std::endl; return false; }

#define ASSERT_FALSE(expr) \
    if (expr) { std::cerr << "  ASSERT_FALSE failed: " << #expr << std::endl; return false; }

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { std::cerr << "  ASSERT_EQ failed: " << #a << " (" << (a) << ") != " << #b << " (" << (b) << ")" << std::endl; return false; }

#define ASSERT_NE(a, b) \
    if ((a) == (b)) { std::cerr << "  ASSERT_NE failed: " << #a << " == " << #b << std::endl; return false; }

#define ASSERT_LT(a, b) \
    if (!((a) < (b))) { std::cerr << "  ASSERT_LT failed: " << #a << " >= " << #b << std::endl; return false; }

#define ASSERT_LE(a, b) \
    if (!((a) <= (b))) { std::cerr << "  ASSERT_LE failed: " << #a << " > " << #b << std::endl; return false; }

#define ASSERT_GT(a, b) \
    if (!((a) > (b))) { std::cerr << "  ASSERT_GT failed: " << #a << " <= " << #b << std::endl; return false; }

#define ASSERT_GE(a, b) \
    if (!((a) >= (b))) { std::cerr << "  ASSERT_GE failed: " << #a << " < " << #b << std::endl; return false; }

#define ASSERT_NEAR(a, b, epsilon) \
    if (std::abs((a) - (b)) > (epsilon)) { std::cerr << "  ASSERT_NEAR failed: |" << (a) << " - " << (b) << "| > " << (epsilon) << std::endl; return false; }

#define ASSERT_STR_EQ(a, b) \
    if ((a) != (b)) { std::cerr << "  ASSERT_STR_EQ failed: \"" << (a) << "\" != \"" << (b) << "\"" << std::endl; return false; }

#endif // TEST_FRAMEWORK_H
