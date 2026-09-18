#pragma once

#include <atomic>
#include <cstddef>

namespace helion::server {

class ConnectionLimit {
 public:
  explicit ConnectionLimit(std::size_t maximum) : maximum_(maximum) {}
  bool tryAcquire();
  void release();
  std::size_t active() const { return active_.load(); }
  std::size_t maximum() const { return maximum_; }

 private:
  const std::size_t maximum_;
  std::atomic<std::size_t> active_{0};
};

} // namespace helion::server
