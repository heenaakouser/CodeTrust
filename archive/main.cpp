#include <iostream>
#include <vector>
using namespace std;

// Function we want to test
int add(int a, int b);


// Information for one test
struct TestCase {
    int a;
    int b;
    int expected;
};

int main() {
    vector<TestCase> tests = {
        {2, 3, 5},
        {-1, 1, 0},
        {0, 0, 0},
        {-2, -4, -6}
    };

    int passed = 0;
    int testNumber = 1;

    cout << "CodeTrust Test Runner Started!" << endl;

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

    cout << "Tests passed: " << passed
         << "/" << tests.size() << endl;

    return 0;
}