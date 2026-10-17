#include "engine.hpp"

#include "hints.hpp"
#include "language.hpp"
#include "report.hpp"
#include "sandbox.hpp"
#include "testcase.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace codetrust {

Engine::Engine(std::string projectRoot) : root_(std::move(projectRoot)) {
  std::error_code ec;
  fs::create_directories(runsDir(), ec);
  fs::create_directories(fs::path(root_) / "data" / "jobs", ec);
}

std::string Engine::runsDir() const {
  return (fs::path(root_) / "data" / "runs").string();
}

std::string Engine::samplesDir() const {
  return (fs::path(root_) / "samples").string();
}

std::string Engine::makeJobDir(const std::string& id) const {
  return (fs::path(root_) / "data" / "jobs" / id).string();
}

std::string Engine::newRunId() const {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const auto t = clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist(0, 0xFFFFFF);

  std::ostringstream id;
  id << std::put_time(&tm, "%Y%m%dT%H%M%S") << "-" << std::hex << std::setw(6)
     << std::setfill('0') << dist(rng);
  return id.str();
}

std::string Engine::nowIso() const {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const auto t = clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

RunReport Engine::execute(const RunRequest& request) {
  RunReport report;
  report.id = newRunId();
  report.createdAt = nowIso();
  report.mode = request.mode;
  report.language = request.language;
  report.label = request.label;
  report.timeoutSec = request.timeoutSec;
  report.memoryMb = request.memoryMb;

  const auto startAll = std::chrono::steady_clock::now();
  const auto langInfo = languageInfo(request.language);

  if (request.source.empty()) {
    report.status = OverallStatus::Error;
    report.summary = "Source code is empty.";
    report.compileLog = report.summary;
    report.suggestion = suggestionForReport(report);
    saveReport(report, runsDir());
    return report;
  }

  if (!dockerAvailable()) {
    report.status = OverallStatus::Error;
    report.summary = "Docker is not available. Start Docker Desktop and retry.";
    report.compileLog = report.summary;
    report.suggestion = suggestionForReport(report);
    saveReport(report, runsDir());
    return report;
  }

  const auto jobDir = makeJobDir(report.id);
  std::error_code ec;
  fs::create_directories(jobDir, ec);

  const auto sourcePath = fs::path(jobDir) / langInfo.sourceFile;
  {
    std::ofstream src(sourcePath);
    src << request.source;
    if (!request.source.empty() && request.source.back() != '\n') {
      src << '\n';
    }
  }

  auto compile = dockerBuild(request.language, jobDir);
  report.compileLog = compile.output;
  report.compileOk = (compile.exitCode == 0);

  if (!report.compileOk) {
    report.status = OverallStatus::CompileError;
    report.summary = langInfo.compiled
                         ? "Compilation failed. Tests were not run."
                         : "Syntax check failed. Tests were not run.";
    report.suggestion = suggestionForReport(report);
    report.totalDurationMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startAll)
            .count();
    saveReport(report, runsDir());
    return report;
  }

  if (request.mode == RunMode::CompileOnly) {
    report.status = OverallStatus::AllPassed;
    report.summary = langInfo.compiled
                         ? "Compilation successful (compile-only mode)."
                         : "Syntax check successful (compile-only mode).";
    report.suggestion = suggestionForReport(report);
    report.totalDurationMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startAll)
            .count();
    saveReport(report, runsDir());
    return report;
  }

  auto cases = parseTestcases(request.testcasesText);
  report.total = static_cast<int>(cases.size());

  if (cases.empty()) {
    report.status = OverallStatus::Failed;
    report.summary = "No valid test cases found. Use input|expected lines.";
    report.suggestion = suggestionForReport(report);
    report.totalDurationMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startAll)
            .count();
    saveReport(report, runsDir());
    return report;
  }

  int index = 1;
  for (const auto& tc : cases) {
    TestResult tr;
    tr.index = index++;
    tr.name = tc.name;
    tr.input = tc.input;
    tr.expected = tc.expected;

    auto run = dockerRun(request.language, jobDir, tc.input, request.timeoutSec,
                         request.memoryMb);
    tr.durationMs = run.durationMs;
    tr.exitCode = run.exitCode;
    tr.actual = normalizeOutput(run.output);
    const auto expectedNorm = normalizeOutput(tc.expected);

    if (run.timedOut) {
      tr.status = TestStatus::Timeout;
      tr.message = "Program exceeded the time limit.";
      ++report.failed;
    } else if (run.exitCode != 0) {
      tr.status = TestStatus::RuntimeError;
      tr.message = "Non-zero exit status: " + std::to_string(run.exitCode);
      ++report.failed;
    } else if (tr.actual == expectedNorm) {
      tr.status = TestStatus::Pass;
      ++report.passed;
    } else {
      tr.status = TestStatus::FailOutput;
      tr.message = "Output did not match expected value.";
      ++report.failed;
    }
    tr.hint = hintForTest(tr);

    report.tests.push_back(std::move(tr));
  }

  report.totalDurationMs =
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - startAll)
          .count();

  if (report.failed == 0) {
    report.status = OverallStatus::AllPassed;
    report.summary = "All tests passed.";
  } else {
    report.status = OverallStatus::Failed;
    report.summary = "One or more tests failed.";
  }
  report.suggestion = suggestionForReport(report);

  saveReport(report, runsDir());

  fs::remove(fs::path(jobDir) / "program", ec);
  fs::remove(fs::path(jobDir) / ".stdin.txt", ec);
  fs::remove(fs::path(jobDir) / "Main.class", ec);

  return report;
}

}  // namespace codetrust
