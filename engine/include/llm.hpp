#pragma once

#include <string>

namespace codetrust {

struct LlmConfig {
  bool configured = false;
  std::string provider;  // groq | gemini
  std::string apiKey;
  std::string model;
  std::string error;
};

struct LlmGenerateResult {
  bool ok = false;
  std::string testcases;
  std::string provider;
  std::string model;
  std::string raw;
  std::string error;
};

struct LlmFixResult {
  bool ok = false;
  std::string fixedSource;
  std::string explanation;
  std::string provider;
  std::string model;
  std::string error;
};

LlmConfig loadLlmConfig();

LlmGenerateResult llmGenerateTests(const std::string& language,
                                   const std::string& source,
                                   const std::string& specification,
                                   int count,
                                   bool inputsOnly = false);

// Suggest a corrected program given compiler/runtime/wrong-answer context.
LlmFixResult llmFixCode(const std::string& language,
                        const std::string& source,
                        const std::string& specification,
                        const std::string& compileLog,
                        const std::string& failureContext);

struct LlmSyncResult {
  bool ok = false;
  std::string source;
  std::string specification;
  std::string error;
};

// Build starter source that implements the problem description.
LlmSyncResult llmSourceFromSpec(const std::string& language,
                                const std::string& specification,
                                const std::string& currentSource);

// Infer a short problem description from source code.
LlmSyncResult llmSpecFromSource(const std::string& language,
                                const std::string& source,
                                const std::string& currentSpec);

}  // namespace codetrust
