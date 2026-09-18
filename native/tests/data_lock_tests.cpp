#include "server/data_lock.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
void check(bool condition, const char* label) {
  if (!condition) throw std::runtime_error(label);
}

bool rejected(const std::string& path) {
  try { helion::server::DataLock lock(path); }
  catch (const std::runtime_error&) { return true; }
  return false;
}
} // namespace

int main() {
  char directory[] = "helion-lock-test-XXXXXX";
  if (!mkdtemp(directory)) return 1;
  const std::string data = std::string(directory) + "/save.db";
  const std::string sidecar = data + ".lock";
  const std::string alias = std::string(directory) + "/alias";
  int result = 0;
  try {
    ino_t inode;
    {
      helion::server::DataLock owner(data);
      struct stat info{};
      check(stat(sidecar.c_str(), &info) == 0 && (info.st_mode & 0777) == 0600, "private lock file");
      inode = info.st_ino;
      check(rejected(data), "duplicate ownership rejected");
      check(rejected(std::string(directory) + "/./save.db"), "equivalent path rejected");
      // Replacing a save must not release ownership of its stable sidecar.
      check(access(data.c_str(), F_OK) != 0, "locking does not create a save file");
    }
    {
      helion::server::DataLock next(data);
      struct stat info{};
      check(stat(sidecar.c_str(), &info) == 0 && info.st_ino == inode, "restart keeps lock inode");
    }
    const pid_t child = fork();
    check(child >= 0, "fork lock owner");
    if (child == 0) {
      try {
        helion::server::DataLock owner(data);
        _exit(0); // No destructor: the OS must release ownership on exit.
      } catch (...) { _exit(1); }
    }
    int status = 0;
    check(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "owner exits without cleanup");
    check(!rejected(data), "ownership released after abrupt exit");
    check(link(sidecar.c_str(), alias.c_str()) == 0, "make hard-link fixture");
    check(rejected(data), "hard-linked lock rejected");
    unlink(alias.c_str());
    unlink(sidecar.c_str());
    check(symlink("alias", sidecar.c_str()) == 0, "make symlink fixture");
    check(rejected(data), "symlink lock rejected");
    check(access(alias.c_str(), F_OK) != 0, "symlink target not created");
    unlink(sidecar.c_str());
    check(mkfifo(sidecar.c_str(), 0600) == 0, "make fifo fixture");
    check(rejected(data), "non-regular lock rejected without blocking");
    std::cout << "data lock tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << "FAILED: " << error.what() << '\n';
    result = 1;
  }
  unlink(alias.c_str());
  unlink(sidecar.c_str());
  rmdir(directory);
  return result;
}
