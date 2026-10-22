// Test-only fault: a verified backup may succeed while the migration commit
// fails with ENOSPC. Never linked into or installed with the native service.
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>
extern "C" int rename(const char* source, const char* destination) {
  using Rename = int (*)(const char*, const char*);
  static const auto actual = reinterpret_cast<Rename>(dlsym(RTLD_NEXT, "rename"));
  const char* target = std::getenv("HELION_TEST_MIGRATION_DESTINATION");
  const char* trigger = std::getenv("HELION_TEST_MIGRATION_AFTER_BACKUP");
  if (target && std::strcmp(target, destination) == 0 && (!trigger || access(trigger, F_OK) == 0)) {
    errno = ENOSPC; return -1;
  }
  if (!actual) { errno = ENOSYS; return -1; }
  return actual(source, destination);
}
