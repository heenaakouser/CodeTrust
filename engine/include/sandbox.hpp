#pragma once

#include "language.hpp"

#include <string>

namespace codetrust {

struct CommandResult {
  int exitCode = 0;
  std::string output;
  bool timedOut = false;
  double durationMs = 0.0;
};

bool dockerAvailable();

// Syntax-check / compile source already written to workDir/<sourceFile>.
CommandResult dockerBuild(Language lang, const std::string& workDir);

// Run the built/interpreted program with stdin = input.
CommandResult dockerRun(Language lang,
                        const std::string& workDir,
                        const std::string& input,
                        int timeoutSec,
                        int memoryMb);

}  // namespace codetrust
