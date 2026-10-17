#pragma once

#include "types.hpp"

#include <string>
#include <vector>

namespace codetrust {

std::vector<TestCase> parseTestcases(const std::string& text);
std::string normalizeOutput(const std::string& s);

}  // namespace codetrust
