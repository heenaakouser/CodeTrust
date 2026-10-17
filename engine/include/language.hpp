#pragma once

#include <string>
#include <vector>

namespace codetrust {

enum class Language {
  Cpp,
  Python,
  Java,
  JavaScript,
  Go
};

struct LanguageInfo {
  Language id;
  std::string key;          // cpp, python, java, javascript, go
  std::string displayName;
  std::string monacoId;
  std::string sourceFile;
  std::string dockerImage;
  bool compiled = true;
};

Language parseLanguage(const std::string& s);
std::string toString(Language lang);
LanguageInfo languageInfo(Language lang);
std::vector<LanguageInfo> allLanguages();
std::string defaultTemplate(Language lang, const std::string& problemKind);

}  // namespace codetrust
