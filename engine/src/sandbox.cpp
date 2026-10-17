#include "sandbox.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <string>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace codetrust {
namespace {

CommandResult runCommand(const std::string& cmd) {
  CommandResult result;
  const auto start = std::chrono::steady_clock::now();

  FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
  if (!pipe) {
    result.exitCode = 127;
    result.output = "Failed to execute command";
    return result;
  }

  std::array<char, 4096> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    result.output += buffer.data();
  }

  const int status = pclose(pipe);
  const auto end = std::chrono::steady_clock::now();
  result.durationMs =
      std::chrono::duration<double, std::milli>(end - start).count();

#if defined(_WIN32)
  result.exitCode = status;
#else
  if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exitCode = 128 + WTERMSIG(status);
  } else {
    result.exitCode = status;
  }
#endif

  return result;
}

std::string shellEscape(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out.push_back(c);
    }
  }
  out += "'";
  return out;
}

std::string buildCommand(Language lang) {
  switch (lang) {
    case Language::Python:
      return "python -m py_compile submission.py";
    case Language::Java:
      return "javac Main.java";
    case Language::JavaScript:
      return "node --check submission.js";
    case Language::Go:
      return "go build -o program submission.go";
    case Language::Cpp:
    default:
      return "g++ -std=c++17 -Wall -Wextra -O2 -o program submission.cpp";
  }
}

std::string runInnerCommand(Language lang, int timeoutSec) {
  const std::string t = std::to_string(timeoutSec);
  switch (lang) {
    case Language::Python:
      return "timeout " + t + "s python /app/submission.py < /stdin.txt";
    case Language::Java:
      return "timeout " + t + "s java -cp /app Main < /stdin.txt";
    case Language::JavaScript:
      return "timeout " + t + "s node /app/submission.js < /stdin.txt";
    case Language::Go:
    case Language::Cpp:
    default:
      return "timeout " + t + "s /app/program < /stdin.txt";
  }
}

bool writeStdinFile(const std::string& workDir, const std::string& input) {
  const std::string inputPath = workDir + "/.stdin.txt";
  FILE* f = fopen(inputPath.c_str(), "wb");
  if (!f) return false;
  fwrite(input.data(), 1, input.size(), f);
  if (!input.empty() && input.back() != '\n') {
    fputc('\n', f);
  }
  fclose(f);
  return true;
}

}  // namespace

bool dockerAvailable() {
  auto r = runCommand("docker info");
  return r.exitCode == 0;
}

CommandResult dockerBuild(Language lang, const std::string& workDir) {
  const auto info = languageInfo(lang);
  std::ostringstream cmd;
  cmd << "docker run --rm "
      << "-v " << shellEscape(workDir) << ":/app -w /app ";

  // Go module cache / toolchain comfort
  if (lang == Language::Go) {
    cmd << "-e GO111MODULE=off ";
  }

  cmd << shellEscape(info.dockerImage) << " "
      << "sh -c " << shellEscape(buildCommand(lang));

  return runCommand(cmd.str());
}

CommandResult dockerRun(Language lang,
                        const std::string& workDir,
                        const std::string& input,
                        int timeoutSec,
                        int memoryMb) {
  if (timeoutSec < 1) timeoutSec = 1;
  if (timeoutSec > 30) timeoutSec = 30;
  if (memoryMb < 32) memoryMb = 32;
  if (memoryMb > 1024) memoryMb = 1024;

  if (!writeStdinFile(workDir, input)) {
    CommandResult err;
    err.exitCode = 1;
    err.output = "Failed to write stdin file";
    return err;
  }

  const auto info = languageInfo(lang);
  const std::string inputPath = workDir + "/.stdin.txt";

  std::ostringstream cmd;
  cmd << "docker run --rm -i "
      << "--network none "
      << "--memory=" << memoryMb << "m "
      << "--cpus=1 "
      << "--pids-limit=64 "
      << "--read-only "
      << "--tmpfs /tmp:rw,nosuid,nodev,size=16m "
      << "-v " << shellEscape(workDir) << ":/app:ro "
      << "-v " << shellEscape(inputPath) << ":/stdin.txt:ro "
      << "-w /tmp "
      << shellEscape(info.dockerImage) << " "
      << "sh -c " << shellEscape(runInnerCommand(lang, timeoutSec));

  auto result = runCommand(cmd.str());
  if (result.exitCode == 124) {
    result.timedOut = true;
  }
  return result;
}

}  // namespace codetrust
