#include "testcase.hpp"

#include <sstream>

namespace codetrust {

std::string normalizeOutput(const std::string& s) {
  std::string out = s;
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ' ||
                          out.back() == '\t')) {
    out.pop_back();
  }
  // Normalize CRLF to LF for comparison
  std::string normalized;
  normalized.reserve(out.size());
  for (size_t i = 0; i < out.size(); ++i) {
    if (out[i] == '\r') {
      if (i + 1 < out.size() && out[i + 1] == '\n') {
        continue;
      }
      normalized.push_back('\n');
    } else {
      normalized.push_back(out[i]);
    }
  }
  return normalized;
}

std::vector<TestCase> parseTestcases(const std::string& text) {
  std::vector<TestCase> cases;
  std::istringstream stream(text);
  std::string line;
  std::string pendingName;
  int autoIndex = 1;

  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (line.empty()) {
      continue;
    }

    if (line[0] == '#') {
      pendingName = line.substr(1);
      while (!pendingName.empty() && pendingName.front() == ' ') {
        pendingName.erase(pendingName.begin());
      }
      continue;
    }

    const auto pipe = line.find('|');
    if (pipe == std::string::npos) {
      continue;
    }

    TestCase tc;
    tc.input = line.substr(0, pipe);
    tc.expected = line.substr(pipe + 1);
    if (!pendingName.empty()) {
      tc.name = pendingName;
      pendingName.clear();
    } else {
      tc.name = "Test " + std::to_string(autoIndex);
    }
    ++autoIndex;
    cases.push_back(std::move(tc));
  }

  return cases;
}

}  // namespace codetrust
