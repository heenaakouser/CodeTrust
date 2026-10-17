#pragma once

#include "language.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace codetrust {

enum class RunMode { Full, CompileOnly };

enum class TestStatus {
  Pass,
  FailOutput,
  RuntimeError,
  Timeout,
  Skipped
};

enum class OverallStatus {
  AllPassed,
  Failed,
  CompileError,
  Error
};

struct TestCase {
  std::string name;
  std::string input;
  std::string expected;
};

struct TestResult {
  int index = 0;
  std::string name;
  TestStatus status = TestStatus::Skipped;
  std::string input;
  std::string expected;
  std::string actual;
  std::string message;
  std::string hint;
  int exitCode = 0;
  double durationMs = 0.0;
};

struct RunRequest {
  std::string source;
  std::string testcasesText;
  Language language = Language::Cpp;
  RunMode mode = RunMode::Full;
  int timeoutSec = 2;
  int memoryMb = 128;
  std::string label;
};

struct RunReport {
  std::string id;
  std::string createdAt;
  OverallStatus status = OverallStatus::Error;
  RunMode mode = RunMode::Full;
  Language language = Language::Cpp;
  std::string label;
  bool compileOk = false;
  std::string compileLog;
  int timeoutSec = 2;
  int memoryMb = 128;
  int total = 0;
  int passed = 0;
  int failed = 0;
  double totalDurationMs = 0.0;
  std::vector<TestResult> tests;
  std::string summary;
  std::string suggestion;
};

inline std::string toString(TestStatus s) {
  switch (s) {
    case TestStatus::Pass: return "PASS";
    case TestStatus::FailOutput: return "FAIL_OUTPUT";
    case TestStatus::RuntimeError: return "RUNTIME_ERROR";
    case TestStatus::Timeout: return "TIMEOUT";
    case TestStatus::Skipped: return "SKIPPED";
  }
  return "UNKNOWN";
}

inline std::string toString(OverallStatus s) {
  switch (s) {
    case OverallStatus::AllPassed: return "ALL_PASSED";
    case OverallStatus::Failed: return "FAILED";
    case OverallStatus::CompileError: return "COMPILE_ERROR";
    case OverallStatus::Error: return "ERROR";
  }
  return "UNKNOWN";
}

inline std::string toString(RunMode m) {
  return m == RunMode::CompileOnly ? "compile_only" : "full";
}

inline RunMode parseMode(const std::string& s) {
  if (s == "compile_only" || s == "compile-only" || s == "compile") {
    return RunMode::CompileOnly;
  }
  return RunMode::Full;
}

}  // namespace codetrust
