#pragma once

#include "types.hpp"

#include <string>

namespace codetrust {

std::string hintForTest(const TestResult& t);
std::string suggestionForReport(const RunReport& report);

}  // namespace codetrust
