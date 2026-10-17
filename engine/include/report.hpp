#pragma once

#include "types.hpp"

#include <string>
#include <vector>

namespace codetrust {

std::string reportToJson(const RunReport& report, bool pretty = true);
std::string reportToText(const RunReport& report);
bool saveReport(const RunReport& report, const std::string& dir);
RunReport loadReport(const std::string& path);
std::vector<RunReport> listReports(const std::string& dir, int limit = 50);

}  // namespace codetrust
