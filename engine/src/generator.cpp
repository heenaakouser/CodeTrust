#include "generator.hpp"

#include <algorithm>
#include <cctype>
#include <random>
#include <sstream>
#include <string>

namespace codetrust {
namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

long long factorial(int n) {
  long long r = 1;
  for (int i = 2; i <= n; ++i) r *= i;
  return r;
}

bool isPalindrome(const std::string& s) {
  for (size_t i = 0, j = s.size(); i < j; ++i) {
    if (s[i] != s[--j]) return false;
  }
  return true;
}

std::string genAdd(int count, std::mt19937& rng) {
  std::ostringstream out;
  // Always include classic edge cases first
  const int edges[][2] = {{0, 0}, {1, -1}, {-5, -7}, {100, 200}, {-1000, 1000}};
  int written = 0;
  for (const auto& e : edges) {
    if (written >= count) break;
    out << "# edge-" << (written + 1) << "\n";
    out << e[0] << " " << e[1] << "|" << (e[0] + e[1]) << "\n";
    ++written;
  }
  std::uniform_int_distribution<int> dist(-5000, 5000);
  while (written < count) {
    const int a = dist(rng);
    const int b = dist(rng);
    out << "# rand-" << (written + 1) << "\n";
    out << a << " " << b << "|" << (a + b) << "\n";
    ++written;
  }
  return out.str();
}

std::string genFactorial(int count, std::mt19937& rng) {
  std::ostringstream out;
  int written = 0;
  for (int n : {0, 1, 5, 10, 12}) {
    if (written >= count) break;
    out << "# n=" << n << "\n";
    out << n << "|" << factorial(n) << "\n";
    ++written;
  }
  std::uniform_int_distribution<int> dist(0, 12);
  while (written < count) {
    const int n = dist(rng);
    out << "# rand-n=" << n << "\n";
    out << n << "|" << factorial(n) << "\n";
    ++written;
  }
  return out.str();
}

std::string genPalindrome(int count, std::mt19937& rng) {
  static const char* words[] = {
      "a", "aa", "ab", "aba", "abba", "radar", "level", "hello",
      "world", "civic", "noon", "test", "xyz", "racecar", "cpp"};
  std::ostringstream out;
  int written = 0;
  for (const char* w : words) {
    if (written >= count) break;
    const std::string s = w;
    out << "# " << s << "\n";
    out << s << "|" << (isPalindrome(s) ? "YES" : "NO") << "\n";
    ++written;
  }
  std::uniform_int_distribution<int> lenDist(1, 8);
  std::uniform_int_distribution<int> chDist(0, 25);
  while (written < count) {
    const int len = lenDist(rng);
    std::string s;
    s.reserve(len);
    for (int i = 0; i < len; ++i) s.push_back(static_cast<char>('a' + chDist(rng)));
    out << "# rand-" << (written + 1) << "\n";
    out << s << "|" << (isPalindrome(s) ? "YES" : "NO") << "\n";
    ++written;
  }
  return out.str();
}

}  // namespace

std::string inferGeneratorKind(const std::string& sampleId,
                               const std::string& title,
                               const std::string& description) {
  const auto blob = lower(sampleId + " " + title + " " + description);
  if (blob.find("factorial") != std::string::npos ||
      blob.find("n!") != std::string::npos) {
    return "factorial";
  }
  if (blob.find("palindrome") != std::string::npos) {
    return "palindrome";
  }
  if (blob.find("add") != std::string::npos ||
      blob.find("sum") != std::string::npos ||
      blob.find("addition") != std::string::npos) {
    return "add";
  }
  return "add";  // safe default for the prototype
}

std::string generateTestcases(const std::string& kind, int count) {
  count = std::clamp(count, 1, 50);
  std::mt19937 rng{std::random_device{}()};
  if (kind == "factorial") return genFactorial(count, rng);
  if (kind == "palindrome") return genPalindrome(count, rng);
  return genAdd(count, rng);
}

DynamicGenerateResult generateTestsDynamic(const DynamicGenerateRequest& req) {
  DynamicGenerateResult out;
  auto llm = llmGenerateTests(req.language, req.source, req.specification,
                              req.count, req.inputsOnly);
  if (llm.ok) {
    out.ok = true;
    out.testcases = llm.testcases;
    out.mode = "llm";
    out.provider = llm.provider;
    out.model = llm.model;
    return out;
  }

  if (req.allowLegacyFallback && !req.legacyKind.empty()) {
    out.ok = true;
    out.testcases = generateTestcases(req.legacyKind, req.count);
    out.mode = "legacy";
    out.provider = "builtin";
    out.model = req.legacyKind;
    out.error = llm.error;  // note why LLM failed
    return out;
  }

  out.ok = false;
  out.error = llm.error.empty() ? "Test generation failed" : llm.error;
  return out;
}

}  // namespace codetrust

