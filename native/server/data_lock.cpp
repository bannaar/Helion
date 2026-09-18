#include "server/data_lock.h"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace helion::server {

DataLock::DataLock(const std::string& dataPath) {
  const std::string path = dataPath + ".lock";
  const int fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
  if (fd < 0) throw std::runtime_error("cannot open persistence lock");
  struct stat info{};
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1 ||
      info.st_uid != geteuid() || fchmod(fd, 0600) != 0) {
    close(fd);
    throw std::runtime_error("cannot protect persistence lock");
  }
  if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
    const int error = errno;
    close(fd);
    throw std::runtime_error(error == EWOULDBLOCK || error == EAGAIN
      ? "persistence file is already in use by another server"
      : "cannot acquire persistence lock");
  }
  fd_ = fd;
}

DataLock::~DataLock() {
  // Keep the sidecar inode stable. Unlinking it could let another process
  // lock a new inode while a waiting process still owns the old one.
  if (fd_ >= 0) close(fd_);
}

} // namespace helion::server
