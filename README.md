# CodeTrust

CodeTrust is a C++ code verification and automated testing prototype.

It compiles a submitted C++ program, runs predefined test cases, compares
actual output with expected output, and saves a test report.

## Current Features

- Compile C++ code using Docker
- Read test cases from `testcases.txt`
- Compare expected and actual output
- Detect compilation errors
- Detect runtime failures and timeouts
- Show passed and failed test counts
- Save results in `report.txt`

## Tech Stack

- C++
- Docker
- Shell scripting
- Git and GitHub

## Project Structure

- `src/submission.cpp` — sample C++ program being tested
- `src/main.cpp` — initial C++ prototype
- `src/test_harness.cpp` — initial function-based test runner
- `testcases.txt` — test inputs and expected outputs
- `run_codetrust.sh` — compiles the program and runs tests
- `report.txt` — generated test report

## How to Run

1. Install and start Docker Desktop.
2. Open Terminal in the project folder.
3. Run:

   `sh run_codetrust.sh`

The script compiles the C++ program, runs the test cases, and saves the
results in `report.txt`.

## Current Limitations

CodeTrust is an early-stage prototype. It currently supports a simple
input/output format and does not prove correctness for every possible input.
It is not yet a production-ready service for running untrusted code.