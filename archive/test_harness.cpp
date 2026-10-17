#include <iostream>
#include <vector>
using namespace std;

// Developer ke submitted code ka function
int add(int a, int b);

// Ek test case ki information
struct TestCase {
    int a;
    int b;
    int expected;
};

int main() {
    // Hamare test cases
    vector<TestCase> tests = {
        {2, 3, 5},
        {-1, 1, 0},
        {0, 0, 0},
        {-2, -4, -6}
    };

    int passed = 0;
    int testNumber = 1;

    cout << "CodeTrust Test Runner Started!" << endl;

    // Har test case ko check karo
    for (const TestCase& test : tests) {
        int actual = add(test.a, test.b);

        if (actual == test.expected) {
            cout << "Test " << testNumber << ": PASS" << endl;
            passed++;
        } else {
            cout << "Test " << testNumber << ": FAIL" << endl;
            cout << "Input: " << test.a << ", " << test.b << endl;
            cout << "Expected: " << test.expected << endl;
            cout << "Actual: " << actual << endl;
        }

        testNumber++;
    }

    // Total result dikhao
    cout << "Tests passed: " << passed
         << "/" << tests.size() << endl;

    // Saare tests pass hue toh 0 return karo
    if (passed == static_cast<int>(tests.size())) {
        return 0;
    }

    // Ek bhi test fail hua toh 1 return karo
    return 1;
}