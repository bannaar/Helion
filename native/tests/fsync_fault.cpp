// Test-only LD_PRELOAD shim, never linked into or installed with HELION.
// The fixture enables a directory-sync failure after normal server startup.
#include <cerrno>
#include <cstdlib>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int fsync(int fd) {
  using Sync = int (*)(int);
  static const auto actual = reinterpret_cast<Sync>(dlsym(RTLD_NEXT, "fsync"));
  struct stat info{};
  const char* trigger = std::getenv("HELION_TEST_FSYNC_FAULT");
  if (trigger && fstat(fd, &info) == 0 && S_ISDIR(info.st_mode) && access(trigger, F_OK) == 0) {
    errno = EIO;
    return -1;
  }
  if (!actual) { errno = ENOSYS; return -1; }
  return actual(fd);
}
