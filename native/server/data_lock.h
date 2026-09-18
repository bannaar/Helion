#pragma once

#include <string>

namespace helion::server {

// Lock a stable sidecar: the data file itself is replaced on every save.
class DataLock {
 public:
  explicit DataLock(const std::string& dataPath);
  ~DataLock();
  DataLock(const DataLock&) = delete;
  DataLock& operator=(const DataLock&) = delete;

 private:
  int fd_ = -1;
};

} // namespace helion::server
