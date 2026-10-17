#include "hints.hpp"

#include <sstream>

namespace codetrust {
namespace {

bool looksNumeric(const std::string& s) {
  if (s.empty()) return false;
  size_t i = 0;
  if (s[0] == '-' || s[0] == '+') i = 1;
  if (i >= s.size()) return false;
  bool digit = false;
  for (; i < s.size(); ++i) {
    if (s[i] >= '0' && s[i] <= '9') {
      digit = true;
    } else if (s[i] == '.') {
      continue;
    } else {
      return false;
    }
  }
  return digit;
}

}  // namespace

std::string hintForTest(const TestResult& t) {
  switch (t.status) {
    case TestStatus::Pass:
      return "";
    case TestStatus::Timeout:
      return "The program did not finish in time. Look for infinite loops, "
             "missing loop conditions, or overly slow algorithms.";
    case TestStatus::RuntimeError:
      return "The program crashed or exited with an error. Check invalid memory "
             "access, division by zero, failed assertions, or unhandled exceptions. "
             "Review stderr/output above for clues.";
    case TestStatus::FailOutput: {
      std::ostringstream tip;
      tip << "Wrong answer. Expected \"" << t.expected << "\" but got \""
          << t.actual << "\".";

      if (t.actual.empty()) {
        tip << " Your program printed nothing — confirm you write to cout and "
               "that input is read correctly with cin.";
      } else if (looksNumeric(t.expected) && looksNumeric(t.actual)) {
        tip << " Both values are numbers — re-check the formula/operator "
               "(e.g. + vs -, off-by-one, wrong variable).";
      } else if (t.actual.find(t.expected) != std::string::npos ||
                 t.expected.find(t.actual) != std::string::npos) {
        tip << " Outputs are similar — check extra spaces, missing newlines, "
               "or YES/NO casing.";
      } else {
        tip << " Re-read the problem statement and compare your logic against "
               "this failing input.";
      }
      tip << " Tip: paste this input alone and debug locally.";
      return tip.str();
    }
    case TestStatus::Skipped:
      return "This test was skipped.";
  }
  return "";
}

std::string suggestionForReport(const RunReport& report) {
  if (report.status == OverallStatus::AllPassed) {
    return "All checks passed for these test cases. This does not prove "
           "correctness for every possible input.";
  }
  if (report.status == OverallStatus::Error) {
    return report.summary.empty()
               ? "CodeTrust could not run the job. Confirm Docker is running."
               : report.summary;
  }
  if (report.status == OverallStatus::CompileError) {
    std::ostringstream tip;
    tip << "Fix the compile errors first, then run again. ";
    // Surface the first error line if present
    const auto& log = report.compileLog;
    const auto pos = log.find("error:");
    if (pos != std::string::npos) {
      auto end = log.find('\n', pos);
      tip << "First error: "
          << log.substr(pos, end == std::string::npos ? std::string::npos
                                                      : end - pos);
    } else {
      tip << "Read the compiler log for undeclared names, missing semicolons, "
             "or wrong includes.";
    }
    return tip.str();
  }

  for (const auto& t : report.tests) {
    if (t.status != TestStatus::Pass) {
      return "Focus on the first failing case (\"" + t.name +
             "\"). " + hintForTest(t);
    }
  }
  return report.summary;
}

}  // namespace codetrust
