#pragma once

#include "types.hpp"

#include <string>

namespace codetrust {

class Engine {
 public:
  explicit Engine(std::string projectRoot);

  RunReport execute(const RunRequest& request);
  std::string runsDir() const;
  std::string samplesDir() const;
  std::string projectRoot() const { return root_; }

 private:
  std::string root_;
  std::string makeJobDir(const std::string& id) const;
  std::string newRunId() const;
  std::string nowIso() const;
};

}  // namespace codetrust
