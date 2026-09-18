#include "server/connection_limit.h"

namespace helion::server {

bool ConnectionLimit::tryAcquire() {
  std::size_t current = active_.load();
  while (current < maximum_) {
    if (active_.compare_exchange_weak(current, current + 1)) return true;
  }
  return false;
}

void ConnectionLimit::release() { active_.fetch_sub(1); }

} // namespace helion::server
