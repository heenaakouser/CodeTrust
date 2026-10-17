#pragma once

#include "llm.hpp"

#include <string>

namespace codetrust {

// Legacy deterministic generators (fallback / demos).
std::string generateTestcases(const std::string& kind, int count = 12);
std::string inferGeneratorKind(const std::string& sampleId,
                               const std::string& title,
                               const std::string& description);

// Primary path: LLM from free-form editor source.
struct DynamicGenerateRequest {
  std::string language;
  std::string source;
  std::string specification;  // what the program should do
  int count = 10;
  bool allowLegacyFallback = false;
  std::string legacyKind;
  // When true, LLM proposes inputs only; caller should fill expected via
  // executing the current source (keeps generated code + tests in sync).
  bool inputsOnly = false;
};

struct DynamicGenerateResult {
  bool ok = false;
  std::string testcases;
  std::string mode;  // llm | legacy
  std::string provider;
  std::string model;
  std::string error;
};

DynamicGenerateResult generateTestsDynamic(const DynamicGenerateRequest& req);

}  // namespace codetrust

