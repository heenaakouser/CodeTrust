#include "report.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace codetrust {
namespace {

json testResultToJson(const TestResult& t) {
  return json{
      {"index", t.index},
      {"name", t.name},
      {"status", toString(t.status)},
      {"input", t.input},
      {"expected", t.expected},
      {"actual", t.actual},
      {"message", t.message},
      {"hint", t.hint},
      {"exitCode", t.exitCode},
      {"durationMs", t.durationMs},
  };
}

TestResult testResultFromJson(const json& j) {
  TestResult t;
  t.index = j.value("index", 0);
  t.name = j.value("name", "");
  const auto status = j.value("status", "SKIPPED");
  if (status == "PASS") t.status = TestStatus::Pass;
  else if (status == "FAIL_OUTPUT") t.status = TestStatus::FailOutput;
  else if (status == "RUNTIME_ERROR") t.status = TestStatus::RuntimeError;
  else if (status == "TIMEOUT") t.status = TestStatus::Timeout;
  else t.status = TestStatus::Skipped;
  t.input = j.value("input", "");
  t.expected = j.value("expected", "");
  t.actual = j.value("actual", "");
  t.message = j.value("message", "");
  t.hint = j.value("hint", "");
  t.exitCode = j.value("exitCode", 0);
  t.durationMs = j.value("durationMs", 0.0);
  return t;
}

json reportToJsonObj(const RunReport& report) {
  json tests = json::array();
  for (const auto& t : report.tests) {
    tests.push_back(testResultToJson(t));
  }

  return json{
      {"id", report.id},
      {"createdAt", report.createdAt},
      {"status", toString(report.status)},
      {"mode", toString(report.mode)},
      {"language", toString(report.language)},
      {"label", report.label},
      {"compileOk", report.compileOk},
      {"compileLog", report.compileLog},
      {"timeoutSec", report.timeoutSec},
      {"memoryMb", report.memoryMb},
      {"total", report.total},
      {"passed", report.passed},
      {"failed", report.failed},
      {"totalDurationMs", report.totalDurationMs},
      {"summary", report.summary},
      {"suggestion", report.suggestion},
      {"tests", tests},
  };
}

RunReport reportFromJsonObj(const json& j) {
  RunReport r;
  r.id = j.value("id", "");
  r.createdAt = j.value("createdAt", "");
  const auto status = j.value("status", "ERROR");
  if (status == "ALL_PASSED") r.status = OverallStatus::AllPassed;
  else if (status == "FAILED") r.status = OverallStatus::Failed;
  else if (status == "COMPILE_ERROR") r.status = OverallStatus::CompileError;
  else r.status = OverallStatus::Error;
  r.mode = parseMode(j.value("mode", "full"));
  r.language = parseLanguage(j.value("language", "cpp"));
  r.label = j.value("label", "");
  r.compileOk = j.value("compileOk", false);
  r.compileLog = j.value("compileLog", "");
  r.timeoutSec = j.value("timeoutSec", 2);
  r.memoryMb = j.value("memoryMb", 128);
  r.total = j.value("total", 0);
  r.passed = j.value("passed", 0);
  r.failed = j.value("failed", 0);
  r.totalDurationMs = j.value("totalDurationMs", 0.0);
  r.summary = j.value("summary", "");
  r.suggestion = j.value("suggestion", "");
  if (j.contains("tests") && j["tests"].is_array()) {
    for (const auto& t : j["tests"]) {
      r.tests.push_back(testResultFromJson(t));
    }
  }
  return r;
}

}  // namespace

std::string reportToJson(const RunReport& report, bool pretty) {
  const auto j = reportToJsonObj(report);
  return pretty ? j.dump(2) : j.dump();
}

std::string reportToText(const RunReport& report) {
  std::ostringstream out;
  out << "CodeTrust Report\n";
  out << "ID: " << report.id << "\n";
  out << "Created: " << report.createdAt << "\n";
  out << "Mode: " << toString(report.mode) << "\n";
  out << "Language: " << toString(report.language) << "\n";
  if (!report.label.empty()) {
    out << "Label: " << report.label << "\n";
  }
  out << "--------------------\n";

  if (report.status == OverallStatus::Error) {
    out << "ERROR\n";
    if (!report.compileLog.empty()) {
      out << report.compileLog << "\n";
    } else if (!report.summary.empty()) {
      out << report.summary << "\n";
    }
    out << "Final result: ERROR\n";
    return out.str();
  }

  if (!report.compileOk) {
    out << "COMPILATION FAILED\n";
    if (!report.compileLog.empty()) {
      out << report.compileLog << "\n";
    }
    out << "Final result: COMPILE_ERROR\n";
    return out.str();
  }

  out << "Compilation successful.\n";
  if (!report.compileLog.empty()) {
    out << report.compileLog << "\n";
  }

  if (report.mode == RunMode::CompileOnly) {
    out << "Compile-only mode: tests were not run.\n";
    out << "Final result: " << toString(report.status) << "\n";
    return out.str();
  }

  for (const auto& t : report.tests) {
    out << t.name << ": " << toString(t.status);
    if (t.durationMs > 0) {
      out << " (" << static_cast<int>(t.durationMs) << " ms)";
    }
    out << "\n";
    if (t.status != TestStatus::Pass) {
      out << "  Input: " << t.input << "\n";
      out << "  Expected: " << t.expected << "\n";
      out << "  Actual: " << t.actual << "\n";
      if (!t.message.empty()) {
        out << "  Message: " << t.message << "\n";
      }
      if (!t.hint.empty()) {
        out << "  Hint: " << t.hint << "\n";
      }
    }
  }

  out << "--------------------\n";
  out << "Total tests: " << report.total << "\n";
  out << "Passed: " << report.passed << "\n";
  out << "Failed: " << report.failed << "\n";
  out << "Duration: " << static_cast<int>(report.totalDurationMs) << " ms\n";
  out << "Final result: " << toString(report.status) << "\n";
  if (!report.summary.empty()) {
    out << report.summary << "\n";
  }
  if (!report.suggestion.empty()) {
    out << "Suggestion: " << report.suggestion << "\n";
  }
  return out.str();
}

bool saveReport(const RunReport& report, const std::string& dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  const auto jsonPath = fs::path(dir) / (report.id + ".json");
  const auto textPath = fs::path(dir) / (report.id + ".txt");

  {
    std::ofstream jf(jsonPath);
    if (!jf) return false;
    jf << reportToJson(report, true);
  }
  {
    std::ofstream tf(textPath);
    if (!tf) return false;
    tf << reportToText(report);
  }
  return true;
}

RunReport loadReport(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    return {};
  }
  json j;
  in >> j;
  return reportFromJsonObj(j);
}

std::vector<RunReport> listReports(const std::string& dir, int limit) {
  std::vector<fs::directory_entry> files;
  std::error_code ec;
  if (!fs::exists(dir, ec)) {
    return {};
  }

  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      files.push_back(entry);
    }
  }

  std::sort(files.begin(), files.end(),
            [](const fs::directory_entry& a, const fs::directory_entry& b) {
              return a.last_write_time() > b.last_write_time();
            });

  std::vector<RunReport> reports;
  for (const auto& entry : files) {
    if (static_cast<int>(reports.size()) >= limit) break;
    auto r = loadReport(entry.path().string());
    if (!r.id.empty()) {
      reports.push_back(std::move(r));
    }
  }
  return reports;
}

}  // namespace codetrust
