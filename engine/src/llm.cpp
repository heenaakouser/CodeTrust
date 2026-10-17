#include "llm.hpp"

#include "json.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

using json = nlohmann::json;

namespace codetrust {
namespace {

std::string shellEscape(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'') out += "'\\''";
    else out.push_back(c);
  }
  out += "'";
  return out;
}

std::string runCapture(const std::string& cmd, int& exitCode) {
  exitCode = 127;
  FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
  if (!pipe) return "Failed to run curl";
  std::string out;
  std::array<char, 4096> buf{};
  while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
    out += buf.data();
  }
  const int status = pclose(pipe);
#if defined(_WIN32)
  exitCode = status;
#else
  if (WIFEXITED(status)) exitCode = WEXITSTATUS(status);
  else exitCode = status;
#endif
  return out;
}

std::string envOr(const char* key, const char* fallback) {
  if (const char* v = std::getenv(key)) {
    if (*v) return v;
  }
  return fallback ? fallback : "";
}

std::string buildPrompt(const std::string& language,
                        const std::string& source,
                        const std::string& specification,
                        int count,
                        bool inputsOnly) {
  std::ostringstream p;
  p << "You are a test-case generator for a programming judge.\n"
    << "Language: " << language << "\n"
    << "Produce exactly " << count
    << " stdin/stdout test cases for the program below.\n\n"
    << "OUTPUT FORMAT RULES (strict):\n"
    << "- Reply with ONLY test lines, no markdown, no commentary.\n"
    << "- Each line: input|expected\n"
    << "- Optional name lines start with # \n"
    << "- Input may contain spaces; do not use | inside input/expected.\n"
    << "- Prefer edge cases and typical cases.\n"
    << "- CRITICAL: Every stdin input MUST match exactly how the SOURCE CODE "
       "reads input (same number of tokens / fields / order). Do not invent a "
       "different I/O layout than the code.\n\n";

  if (inputsOnly) {
    p << "MODE: INPUTS ONLY.\n"
      << "Put a single underscore as the expected placeholder: input|_\n"
      << "Focus on varied, valid inputs the program can read without hanging.\n"
      << "Do not invent expected answers.\n\n";
  } else if (!specification.empty()) {
    p << "PROGRAM SPECIFICATION (source of truth for expected outputs):\n"
      << specification << "\n\n"
      << "Use the CODE to infer I/O format. Expected outputs MUST follow "
         "the specification (correct answers), not bugs in the code.\n\n";
  } else {
    p << "No separate specification was provided. Infer the intended behavior "
         "from the code (assume the author meant a correct solution) and "
         "generate tests accordingly.\n\n";
  }

  p << "SOURCE CODE:\n```" << language << "\n" << source << "\n```\n";
  return p.str();
}

std::string jsonTextField(const json& node) {
  if (node.is_string()) return node.get<std::string>();
  if (node.is_array()) {
    std::string out;
    for (const auto& part : node) {
      if (part.is_string())
        out += part.get<std::string>();
      else if (part.is_object() && part.contains("text") && part["text"].is_string())
        out += part["text"].get<std::string>();
    }
    return out;
  }
  return {};
}

std::string extractMessageContent(const json& response) {
  // OpenAI / Groq shape (incl. gpt-oss reasoning models)
  if (response.contains("choices") && response["choices"].is_array() &&
      !response["choices"].empty()) {
    const auto& c0 = response["choices"][0];
    if (c0.contains("message") && c0["message"].is_object()) {
      const auto& msg = c0["message"];
      if (msg.contains("content")) {
        auto content = jsonTextField(msg["content"]);
        if (!content.empty()) return content;
      }
      // Some reasoning models put the answer in reasoning when content is empty.
      for (const char* key : {"reasoning", "reasoning_content"}) {
        if (msg.contains(key)) {
          auto reasoning = jsonTextField(msg[key]);
          if (!reasoning.empty()) return reasoning;
        }
      }
    }
    if (c0.contains("text")) return jsonTextField(c0["text"]);
  }
  // Gemini shape
  if (response.contains("candidates") && response["candidates"].is_array() &&
      !response["candidates"].empty()) {
    const auto& c0 = response["candidates"][0];
    if (c0.contains("content") && c0["content"].contains("parts") &&
        c0["content"]["parts"].is_array() && !c0["content"]["parts"].empty()) {
      return c0["content"]["parts"][0].value("text", "");
    }
  }
  return {};
}

std::string sanitizeTestcases(std::string text) {
  // Strip markdown fences if the model ignored instructions
  const auto fence = text.find("```");
  if (fence != std::string::npos) {
    auto start = text.find('\n', fence);
    auto end = text.rfind("```");
    if (start != std::string::npos && end != std::string::npos && end > start) {
      text = text.substr(start + 1, end - start - 1);
    }
  }

  std::istringstream in(text);
  std::ostringstream out;
  std::string line;
  int kept = 0;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    if (line[0] == '#') {
      out << line << "\n";
      continue;
    }
    if (line.find('|') == std::string::npos) continue;
    out << line << "\n";
    ++kept;
  }
  if (kept == 0) return {};
  return out.str();
}

LlmGenerateResult callGroq(const LlmConfig& cfg, const std::string& prompt) {
  LlmGenerateResult result;
  result.provider = "groq";
  result.model = cfg.model;

  json body = {
      {"model", cfg.model},
      {"temperature", 0.2},
      {"max_completion_tokens", 4096},
      {"messages",
       json::array(
           {{{"role", "system"},
             {"content",
              "You generate programming contest style I/O tests. "
              "Output only input|expected lines."}},
            {{"role", "user"}, {"content", prompt}}})},
  };
  // gpt-oss reasoning models otherwise spend the token budget on thinking.
  if (cfg.model.find("gpt-oss") != std::string::npos) {
    body["reasoning_effort"] = "low";
    body["include_reasoning"] = false;
  }

  const std::string payload = body.dump();
  const std::string cmd =
      "curl -sS --max-time 60 "
      "https://api.groq.com/openai/v1/chat/completions "
      "-H " +
      shellEscape("Authorization: Bearer " + cfg.apiKey) + " -H " +
      shellEscape("Content-Type: application/json") + " -d " +
      shellEscape(payload);

  int code = 0;
  result.raw = runCapture(cmd, code);
  if (code != 0) {
    result.error = "curl failed (" + std::to_string(code) + "): " + result.raw;
    return result;
  }

  json response;
  try {
    response = json::parse(result.raw);
  } catch (...) {
    result.error = "Invalid JSON from Groq";
    return result;
  }
  if (response.contains("error")) {
    result.error = response["error"].value("message", response.dump());
    return result;
  }

  const auto content = extractMessageContent(response);
  result.testcases = sanitizeTestcases(content);
  if (result.testcases.empty()) {
    result.error = "LLM returned no valid input|expected lines";
    return result;
  }
  result.ok = true;
  return result;
}

LlmGenerateResult callGemini(const LlmConfig& cfg, const std::string& prompt) {
  LlmGenerateResult result;
  result.provider = "gemini";
  result.model = cfg.model;

  json body = {
      {"contents",
       json::array({{{"parts", json::array({{{"text", prompt}}})}}})},
      {"generationConfig", {{"temperature", 0.2}}},
  };

  const std::string url =
      "https://generativelanguage.googleapis.com/v1beta/models/" + cfg.model +
      ":generateContent?key=" + cfg.apiKey;

  const std::string cmd =
      "curl -sS --max-time 60 " + shellEscape(url) + " -H " +
      shellEscape("Content-Type: application/json") + " -d " +
      shellEscape(body.dump());

  int code = 0;
  result.raw = runCapture(cmd, code);
  if (code != 0) {
    result.error = "curl failed (" + std::to_string(code) + "): " + result.raw;
    return result;
  }

  json response;
  try {
    response = json::parse(result.raw);
  } catch (...) {
    result.error = "Invalid JSON from Gemini";
    return result;
  }
  if (response.contains("error")) {
    result.error = response["error"].value("message", response.dump());
    return result;
  }

  const auto content = extractMessageContent(response);
  result.testcases = sanitizeTestcases(content);
  if (result.testcases.empty()) {
    result.error = "LLM returned no valid input|expected lines";
    return result;
  }
  result.ok = true;
  return result;
}

}  // namespace

LlmConfig loadLlmConfig() {
  LlmConfig cfg;
  cfg.provider = envOr("CODETRUST_LLM_PROVIDER", "groq");
  cfg.apiKey = envOr("CODETRUST_LLM_API_KEY", "");
  if (cfg.apiKey.empty()) {
    // Common aliases
    cfg.apiKey = envOr("GROQ_API_KEY", "");
    if (cfg.apiKey.empty()) cfg.apiKey = envOr("GEMINI_API_KEY", "");
  }

  if (cfg.provider == "gemini") {
    cfg.model = envOr("CODETRUST_LLM_MODEL", "gemini-2.0-flash");
  } else {
    cfg.provider = "groq";
    cfg.model = envOr("CODETRUST_LLM_MODEL", "openai/gpt-oss-20b");
  }

  if (cfg.apiKey.empty()) {
    cfg.configured = false;
    cfg.error =
        "No LLM API key. Set CODETRUST_LLM_API_KEY (or GROQ_API_KEY). "
        "Free key: https://console.groq.com/keys";
  } else {
    cfg.configured = true;
  }
  return cfg;
}

LlmGenerateResult llmGenerateTests(const std::string& language,
                                   const std::string& source,
                                   const std::string& specification,
                                   int count,
                                   bool inputsOnly) {
  LlmGenerateResult result;
  auto cfg = loadLlmConfig();
  if (!cfg.configured) {
    result.error = cfg.error;
    return result;
  }
  if (source.empty()) {
    result.error = "Source code is empty — write code in the editor first.";
    return result;
  }
  if (count < 1) count = 1;
  if (count > 30) count = 30;

  const auto prompt =
      buildPrompt(language, source, specification, count, inputsOnly);
  if (cfg.provider == "gemini") return callGemini(cfg, prompt);
  return callGroq(cfg, prompt);
}

namespace {

std::string stripCodeFences(std::string text) {
  const auto start = text.find("```");
  if (start == std::string::npos) return text;
  auto after = text.find('\n', start);
  if (after == std::string::npos) return text;
  const auto end = text.find("```", after + 1);
  if (end == std::string::npos) return text;
  return text.substr(after + 1, end - after - 1);
}

// Models sometimes return source with literal "\n" instead of real newlines.
std::string normalizeCodeText(std::string s) {
  // Trim wrapping quotes
  if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
                        (s.front() == '\'' && s.back() == '\''))) {
    s = s.substr(1, s.size() - 2);
  }

  size_t realNl = 0;
  size_t escNl = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\n') ++realNl;
    if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') ++escNl;
  }

  // One long line (or almost) with many "\n" tokens → treat as mis-escaped.
  if (escNl >= 2 && (escNl > realNl || realNl <= 1)) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
      if (s[i] == '\\' && i + 1 < s.size()) {
        const char n = s[i + 1];
        if (n == 'n') {
          out.push_back('\n');
          ++i;
          continue;
        }
        if (n == 't') {
          out.push_back('\t');
          ++i;
          continue;
        }
        if (n == 'r') {
          out.push_back('\r');
          ++i;
          continue;
        }
        if (n == '"' || n == '\\' || n == '\'') {
          out.push_back(n);
          ++i;
          continue;
        }
      }
      out.push_back(s[i]);
    }
    s = std::move(out);
  }

  // Drop accidental trailing spaces on each line
  std::string cleaned;
  cleaned.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\n') {
      while (!cleaned.empty() &&
             (cleaned.back() == ' ' || cleaned.back() == '\t')) {
        cleaned.pop_back();
      }
      cleaned.push_back('\n');
    } else {
      cleaned.push_back(s[i]);
    }
  }
  if (!cleaned.empty() && cleaned.back() != '\n') cleaned.push_back('\n');
  return cleaned;
}

std::string buildFixPrompt(const std::string& language,
                           const std::string& source,
                           const std::string& specification,
                           const std::string& compileLog,
                           const std::string& failureContext) {
  std::ostringstream p;
  p << "You are an expert " << language
    << " judge assistant. Fix the submission so it passes the failing tests.\n"
    << "Return ONLY a JSON object (no markdown) with keys:\n"
    << "  explanation: short plain-English root cause and fix\n"
    << "  fixedSource: the FULL corrected source code\n\n"
    << "RULES (strict):\n"
    << "1. The PROBLEM SPECIFICATION defines intent.\n"
    << "2. The FAILING TESTS define the exact stdin/stdout contract — expected "
       "stdout is ground truth. Do NOT invent a different I/O format.\n"
    << "3. Infer parsing from the sample inputs (e.g. first token may be n; "
       "array then target; operation scripts for class APIs).\n"
    << "4. After the fix, the program must print exactly the expected stdout "
       "for every failing case (and keep passing ones working).\n"
    << "5. Prefer the smallest correct change; keep a complete runnable "
       "program with main / entrypoint.\n"
    << "6. Prefer an optimized algorithm when it stays simple (e.g. O(n) hash "
       "map). Keep code SHORT — no dead branches, unused flags, or dual I/O "
       "formats.\n"
    << "7. Output readable multi-line indented code in fixedSource.\n\n";
  if (!specification.empty()) {
    p << "PROBLEM SPECIFICATION:\n" << specification << "\n\n";
  } else {
    p << "PROBLEM SPECIFICATION: (not provided — infer from tests and code)\n\n";
  }
  if (!compileLog.empty()) {
    p << "COMPILER / BUILD LOG:\n" << compileLog << "\n\n";
  }
  if (!failureContext.empty()) {
    p << "JUDGE RESULTS / I/O EXAMPLES:\n" << failureContext << "\n\n";
  }
  p << "BROKEN SOURCE:\n```" << language << "\n" << source << "\n```\n";
  return p.str();
}

std::string chatContent(const LlmConfig& cfg,
                        const std::string& system,
                        const std::string& userPrompt,
                        std::string& err,
                        std::string& raw) {
  if (cfg.provider == "gemini") {
    json body = {
        {"contents",
         json::array(
             {{{"parts",
                json::array({{{"text", system + "\n\n" + userPrompt}}})}}})},
        {"generationConfig", {{"temperature", 0.1}}},
    };
    const std::string url =
        "https://generativelanguage.googleapis.com/v1beta/models/" + cfg.model +
        ":generateContent?key=" + cfg.apiKey;
    const std::string cmd =
        "curl -sS --max-time 90 " + shellEscape(url) + " -H " +
        shellEscape("Content-Type: application/json") + " -d " +
        shellEscape(body.dump());
    int code = 0;
    raw = runCapture(cmd, code);
    if (code != 0) {
      err = "Request failed";
      return {};
    }
    try {
      auto response = json::parse(raw);
      if (response.contains("error")) {
        err = response["error"].value("message", "Provider error");
        return {};
      }
      return extractMessageContent(response);
    } catch (...) {
      err = "Invalid provider response";
      return {};
    }
  }

  json body = {
      {"model", cfg.model},
      {"temperature", 0.1},
      {"max_completion_tokens", 8192},
      {"messages",
       json::array({{{"role", "system"}, {"content", system}},
                    {{"role", "user"}, {"content", userPrompt}}})},
  };
  if (cfg.model.find("gpt-oss") != std::string::npos) {
    body["reasoning_effort"] = "low";
    body["include_reasoning"] = false;
  }
  const std::string cmd =
      "curl -sS --max-time 120 "
      "https://api.groq.com/openai/v1/chat/completions "
      "-H " +
      shellEscape("Authorization: Bearer " + cfg.apiKey) + " -H " +
      shellEscape("Content-Type: application/json") + " -d " +
      shellEscape(body.dump());
  int code = 0;
  raw = runCapture(cmd, code);
  if (code != 0) {
    err = "Request failed";
    return {};
  }
  try {
    auto response = json::parse(raw);
    if (response.contains("error")) {
      err = response["error"].value("message", "Provider error");
      return {};
    }
    auto content = extractMessageContent(response);
    if (content.empty()) {
      std::string finish;
      if (response.contains("choices") && response["choices"].is_array() &&
          !response["choices"].empty()) {
        finish = response["choices"][0].value("finish_reason", "");
      }
      err = finish == "length"
                ? "Model ran out of tokens before finishing the code. Try again."
                : "Model returned an empty response. Try again.";
    }
    return content;
  } catch (...) {
    err = "Invalid provider response";
    return {};
  }
}

}  // namespace

LlmFixResult llmFixCode(const std::string& language,
                        const std::string& source,
                        const std::string& specification,
                        const std::string& compileLog,
                        const std::string& failureContext) {
  LlmFixResult result;
  auto cfg = loadLlmConfig();
  result.provider = cfg.provider;
  result.model = cfg.model;
  if (!cfg.configured) {
    result.error = "Fix suggestions are not configured on this server.";
    return result;
  }
  if (source.empty()) {
    result.error = "Source code is empty.";
    return result;
  }

  const auto prompt = buildFixPrompt(language, source, specification, compileLog,
                                     failureContext);
  std::string err;
  std::string raw;
  auto content = chatContent(
      cfg,
      "You fix programming submissions to pass an automated judge. "
      "Expected test outputs are ground truth for I/O. Reply with JSON only.",
      prompt, err, raw);
  if (content.empty()) {
    result.error = err.empty() ? "Could not generate a fix." : err;
    return result;
  }

  // Prefer JSON object
  try {
    auto start = content.find('{');
    auto end = content.rfind('}');
    if (start != std::string::npos && end != std::string::npos && end > start) {
      auto obj = json::parse(content.substr(start, end - start + 1));
      result.explanation = obj.value("explanation", "");
      result.fixedSource = obj.value("fixedSource", "");
    }
  } catch (...) {
  }

  if (result.fixedSource.empty()) {
    result.fixedSource = stripCodeFences(content);
    if (result.explanation.empty()) {
      result.explanation = "Applied an automated fix based on the reported error.";
    }
  }
  result.fixedSource = normalizeCodeText(result.fixedSource);

  if (result.fixedSource.empty() || result.fixedSource == source) {
    result.error = "No useful fix was produced. Try simplifying the error or description.";
    return result;
  }

  result.ok = true;
  return result;
}

LlmSyncResult llmSourceFromSpec(const std::string& language,
                                const std::string& specification,
                                const std::string& currentSource) {
  LlmSyncResult result;
  auto cfg = loadLlmConfig();
  if (!cfg.configured) {
    result.error = "Sync is not configured on this server.";
    return result;
  }
  if (specification.empty()) {
    result.error = "Problem description is empty.";
    return result;
  }

  std::ostringstream prompt;
  prompt << "Write a complete, correct, OPTIMIZED " << language
         << " solution for the problem below.\n"
         << "If the problem is class/API based (e.g. LRUCache), implement the "
            "required class/methods and a small main that reads a simple "
            "operation script from stdin and prints results to stdout so it "
            "can be tested.\n"
         << "If the problem is pure I/O, use stdin/stdout only.\n"
         << "STYLE (strict):\n"
         << "- Prefer the asymptotically best practical approach "
            "(e.g. hash map O(n) over nested loops when appropriate).\n"
         << "- Keep the code SHORT: no dead code, no unused flags/branches, "
            "no verbose comments, no alternate paths for unused edge formats.\n"
         << "- One clear I/O convention; do not branch on multiple input styles.\n"
         << "- Readable multi-line indented code; typically under ~60 lines "
            "unless the problem truly needs more.\n"
         << "Return ONLY a JSON object with key fixedSource (full source code).\n\n"
         << "PROBLEM:\n" << specification << "\n\n";
  if (!currentSource.empty()) {
    prompt << "CURRENT CODE (replace with a clean optimized solution):\n```"
           << language << "\n"
           << currentSource << "\n```\n";
  }

  std::string err;
  std::string raw;
  auto content = chatContent(
      cfg,
      "You generate short, optimized programming solutions. Reply with JSON only.",
      prompt.str(), err, raw);
  if (content.empty()) {
    result.error = err.empty() ? "Could not update code from problem." : err;
    return result;
  }

  try {
    auto start = content.find('{');
    auto end = content.rfind('}');
    if (start != std::string::npos && end != std::string::npos && end > start) {
      auto obj = json::parse(content.substr(start, end - start + 1));
      result.source = obj.value("fixedSource", "");
      if (result.source.empty()) result.source = obj.value("source", "");
    }
  } catch (...) {
  }
  if (result.source.empty()) result.source = stripCodeFences(content);
  result.source = normalizeCodeText(result.source);
  if (result.source.empty()) {
    result.error = "No source code was produced.";
    return result;
  }
  result.ok = true;
  result.specification = specification;
  return result;
}

LlmSyncResult llmSpecFromSource(const std::string& language,
                                const std::string& source,
                                const std::string& currentSpec) {
  LlmSyncResult result;
  auto cfg = loadLlmConfig();
  if (!cfg.configured) {
    result.error = "Sync is not configured on this server.";
    return result;
  }
  if (source.empty()) {
    result.error = "Source code is empty.";
    return result;
  }

  std::ostringstream prompt;
  prompt << "Read this " << language
         << " program and write a clear 1-2 sentence problem description "
            "of what it should do (stdin/stdout behavior).\n"
         << "Return ONLY a JSON object with key specification.\n\n"
         << "SOURCE:\n```" << language << "\n"
         << source << "\n```\n";
  if (!currentSpec.empty()) {
    prompt << "\nCURRENT DESCRIPTION (improve/replace if mismatched):\n"
           << currentSpec << "\n";
  }

  std::string err;
  std::string raw;
  auto content = chatContent(
      cfg, "You write concise programming problem statements. JSON only.",
      prompt.str(), err, raw);
  if (content.empty()) {
    result.error = err.empty() ? "Could not update problem from code." : err;
    return result;
  }

  try {
    auto start = content.find('{');
    auto end = content.rfind('}');
    if (start != std::string::npos && end != std::string::npos && end > start) {
      auto obj = json::parse(content.substr(start, end - start + 1));
      result.specification = obj.value("specification", "");
      if (result.specification.empty()) {
        result.specification = obj.value("description", "");
      }
    }
  } catch (...) {
  }
  if (result.specification.empty()) {
    result.specification = stripCodeFences(content);
    // strip quotes if the model returned a bare string
    while (!result.specification.empty() &&
           (result.specification.front() == '"' ||
            result.specification.front() == '\'')) {
      result.specification.erase(result.specification.begin());
    }
    while (!result.specification.empty() &&
           (result.specification.back() == '"' ||
            result.specification.back() == '\'' ||
            result.specification.back() == '\n')) {
      result.specification.pop_back();
    }
  }
  if (result.specification.empty()) {
    result.error = "No problem description was produced.";
    return result;
  }
  result.ok = true;
  result.source = source;
  return result;
}

}  // namespace codetrust

