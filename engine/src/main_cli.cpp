#include "engine.hpp"
#include "language.hpp"
#include "report.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

std::string readFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("Cannot read file: " + path);
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void printUsage(const char* argv0) {
  std::cerr
      << "Usage: " << argv0 << " [options]\n"
      << "  --source <file>     Source file (required)\n"
      << "  --language <id>     cpp|python|java|javascript|go (default cpp)\n"
      << "  --tests <file>      Test cases file (required for full mode)\n"
      << "  --mode <full|compile_only>\n"
      << "  --timeout <sec>     Per-test timeout (default 2)\n"
      << "  --memory <mb>       Memory limit (default 128)\n"
      << "  --label <text>      Optional run label\n"
      << "  --root <dir>        Project root (default: cwd)\n"
      << "  --json              Print JSON report to stdout\n"
      << "  --help              Show help\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string sourcePath;
  std::string testsPath;
  std::string mode = "full";
  std::string language = "cpp";
  std::string label;
  std::string root = fs::current_path().string();
  int timeoutSec = 2;
  int memoryMb = 128;
  bool asJson = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto need = [&](const char* name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error(std::string("Missing value for ") + name);
      }
      return argv[++i];
    };

    try {
      if (arg == "--help" || arg == "-h") {
        printUsage(argv[0]);
        return 0;
      } else if (arg == "--source") {
        sourcePath = need("--source");
      } else if (arg == "--language" || arg == "--lang") {
        language = need("--language");
      } else if (arg == "--tests") {
        testsPath = need("--tests");
      } else if (arg == "--mode") {
        mode = need("--mode");
      } else if (arg == "--timeout") {
        timeoutSec = std::stoi(need("--timeout"));
      } else if (arg == "--memory") {
        memoryMb = std::stoi(need("--memory"));
      } else if (arg == "--label") {
        label = need("--label");
      } else if (arg == "--root") {
        root = need("--root");
      } else if (arg == "--json") {
        asJson = true;
      } else {
        std::cerr << "Unknown argument: " << arg << "\n";
        printUsage(argv[0]);
        return 2;
      }
    } catch (const std::exception& ex) {
      std::cerr << ex.what() << "\n";
      return 2;
    }
  }

  if (sourcePath.empty()) {
    std::cerr << "--source is required\n";
    printUsage(argv[0]);
    return 2;
  }

  try {
    codetrust::RunRequest req;
    req.source = readFile(sourcePath);
    req.language = codetrust::parseLanguage(language);
    req.mode = codetrust::parseMode(mode);
    req.timeoutSec = timeoutSec;
    req.memoryMb = memoryMb;
    req.label = label;

    if (req.mode == codetrust::RunMode::Full) {
      if (testsPath.empty()) {
        std::cerr << "--tests is required in full mode\n";
        return 2;
      }
      req.testcasesText = readFile(testsPath);
    }

    codetrust::Engine engine(root);
    auto report = engine.execute(req);

    if (asJson) {
      std::cout << codetrust::reportToJson(report, true) << "\n";
    } else {
      std::cout << codetrust::reportToText(report);
      std::cout << "Saved: " << engine.runsDir() << "/" << report.id
                << ".json\n";
    }

    if (report.status == codetrust::OverallStatus::AllPassed) {
      return 0;
    }
    if (report.status == codetrust::OverallStatus::CompileError) {
      return 1;
    }
    return 1;
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 2;
  }
}
