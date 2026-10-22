#include "server/operations.h"
#include <cerrno>
#include <ctime>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace helion::server {
std::string_view phaseName(ServicePhase phase) {
  switch (phase) {
    case ServicePhase::starting: return "STARTING";
    case ServicePhase::validating: return "VALIDATING";
    case ServicePhase::loading: return "LOADING";
    case ServicePhase::shutting_down: return "SHUTTING_DOWN";
    case ServicePhase::fatal: return "FATAL";
    case ServicePhase::online: return "ONLINE";
    case ServicePhase::maintenance_soon: return "MAINTENANCE_SOON";
    case ServicePhase::draining: return "DRAINING";
    case ServicePhase::maintenance: return "MAINTENANCE";
    case ServicePhase::degraded: return "DEGRADED";
    case ServicePhase::offline: return "OFFLINE";
  }
  return "DEGRADED";
}
void ServiceLifecycle::online() { phase = ServicePhase::online; }
void ServiceLifecycle::maintenance(double now, unsigned warningSeconds) {
  if (phase == ServicePhase::online) { phase = ServicePhase::maintenance_soon; deadline_ = now + warningSeconds; }
}
void ServiceLifecycle::drain(double now, unsigned drainSeconds) {
  if (phase == ServicePhase::online || phase == ServicePhase::maintenance_soon || phase == ServicePhase::starting) {
    phase = ServicePhase::draining; deadline_ = now + drainSeconds;
  }
}
bool ServiceLifecycle::advance(double now, unsigned drainSeconds) {
  if (phase == ServicePhase::maintenance_soon && now >= deadline_) drain(now, drainSeconds);
  return phase == ServicePhase::draining && now >= deadline_;
}
bool ServiceLifecycle::ready() const { return phase == ServicePhase::online || phase == ServicePhase::maintenance_soon; }
void operationLog(const EnvironmentPolicy& policy, std::string_view event, std::string_view result) {
  // Only static event/result identifiers, trusted environment and validated instance.
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  std::ostringstream out;
  out << "{\"timestamp\":" << std::time(nullptr) << ",\"subsystem\":\"operations\",\"environment\":\""
    << runtimeEnvironmentName(policy.environment) << "\",\"instance\":\"" << policy.instance
    << "\",\"pid\":" << getpid() << ",\"event\":\"" << event << "\",\"result\":\"" << result << "\"}\n";
  // One small write keeps a JSON record intact when legacy diagnostic stderr
  // writes occur concurrently. Avoid iostream's fragmented field-by-field I/O.
  const auto line = out.str();
  ssize_t written;
  do { written = write(STDERR_FILENO, line.data(), line.size()); } while (written < 0 && errno == EINTR);
}
void syncDirectory(const std::string& path) {
  const auto parent = std::filesystem::absolute(path).parent_path();
  const int fd = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) throw std::runtime_error("cannot open durability directory");
  const int result = fsync(fd);
  close(fd);
  if (result) throw std::runtime_error("cannot sync durability directory");
}
void replacePrivateFile(const std::string& path, std::string_view content, bool durable) {
  std::string temp = path + ".tmp.XXXXXX";
  const int fd = mkstemp(temp.data());
  if (fd < 0) throw std::runtime_error("cannot stage private file");
  bool closed = false;
  try {
    while (!content.empty()) {
      const auto written = write(fd, content.data(), content.size());
      if (written < 0 && errno == EINTR) continue;
      if (written <= 0) throw std::runtime_error("cannot write private file");
      content.remove_prefix(static_cast<std::size_t>(written));
    }
    if (durable && fsync(fd)) throw std::runtime_error("cannot sync private file");
    const int result = close(fd);
    closed = true;
    if (result) throw std::runtime_error("cannot close private file");
    if (rename(temp.c_str(), path.c_str())) throw std::runtime_error("cannot replace private file");
    if (durable) syncDirectory(path);
  } catch (...) { if (!closed) close(fd); unlink(temp.c_str()); throw; }
}
void publishStatus(const std::string& data, const EnvironmentPolicy& policy, ServicePhase phase,
    const ServiceMetrics& m, std::uint64_t uptime, std::size_t connections, std::size_t players, bool durable) {
  const bool ready = durable && (phase == ServicePhase::online || phase == ServicePhase::maintenance_soon);
  std::ostringstream out;
  out << "{\"schema\":1,\"timestamp\":" << std::time(nullptr) << ",\"pid\":" << getpid()
    << ",\"environment\":\"" << runtimeEnvironmentName(policy.environment) << "\",\"instance\":\"" << policy.instance
    << "\",\"channel\":\"" << policy.releaseChannel() << "\",\"phase\":\"" << phaseName(phase)
    << "\",\"ready\":" << (ready ? "true" : "false") << ",\"durable\":" << (durable ? "true" : "false")
    << ",\"uptime_seconds\":" << uptime << ",\"connections\":" << connections << ",\"players\":" << players
    << ",\"connections_total\":" << m.connections << ",\"rejected_total\":" << m.rejected
    << ",\"requests_total\":" << m.requests << ",\"malformed_total\":" << m.malformed
    << ",\"auth_failures_total\":" << m.authFailures << ",\"saves_total\":" << m.saves
    << ",\"save_failures_total\":" << m.saveFailures << ",\"save_microseconds_total\":" << m.saveMicros << ",\"last_save_timestamp\":" << m.lastSave
    << ",\"backups_total\":" << m.backups << ",\"backup_failures_total\":" << m.backupFailures
    << ",\"last_backup_timestamp\":" << m.lastBackup << ",\"load_microseconds\":" << m.loadMicros << "}\n";
  // Freshness is the health contract. Ephemeral status needs atomic publication,
  // not disk durability; fsync here would contend with authoritative saves.
  replacePrivateFile(data + ".status.json", out.str(), false);
}
namespace {
std::string backupPath(const std::string& data, const std::string& id) {
  if (!safeIdentifier(id, 80)) throw std::runtime_error("invalid backup id");
  const auto directory = std::filesystem::absolute(data).parent_path() / ".helion-backups";
  if (mkdir(directory.c_str(), 0700) && errno != EEXIST) throw std::runtime_error("cannot create backup directory");
  struct stat info{};
  if (lstat(directory.c_str(), &info) || !S_ISDIR(info.st_mode) || info.st_uid != geteuid() || (info.st_mode & 077))
    throw std::runtime_error("unsafe backup directory");
  return (directory / (id + ".snapshot")).string();
}
std::string identity(const EnvironmentPolicy& policy) {
  return std::string(runtimeEnvironmentName(policy.environment)) + " " + policy.instance + " " +
    (policy.cheapEconomy() ? "convenience" : "production-like");
}
}
std::string uniqueBackupId(std::string_view prefix) {
  return std::string(prefix) + "-" + std::to_string(std::time(nullptr)) + "-" + std::to_string(getpid());
}
std::string createBackup(const std::string& data, const EnvironmentPolicy& policy, const std::string& id) {
  const auto payload = readPrivateFile(data, 64 * 1024 * 1024);
  const auto path = backupPath(data, id);
  const auto header = "HELION_BACKUP_V1 " + identity(policy) + " " + id + " " +
    std::to_string(std::time(nullptr)) + " persistence-admin1 native-0.2.0 " + digest(payload) + "\n";
  std::string temp = path + ".tmp.XXXXXX";
  const int fd = mkstemp(temp.data());
  if (fd < 0) throw std::runtime_error("cannot stage backup");
  close(fd);
  try {
    writePrivateFile(temp, header + payload, false);
    // Atomic no-overwrite publication, including simultaneous/retried IDs.
    if (link(temp.c_str(), path.c_str())) throw std::runtime_error("cannot publish backup (id may exist)");
    unlink(temp.c_str());
    syncDirectory(path);
    if (readBackup(data, policy, id) != payload) throw std::runtime_error("backup verification failed");
  } catch (...) { unlink(temp.c_str()); throw; }
  syncDirectory(std::filesystem::path(path).parent_path().string());
  operationLog(policy, "backup", "created");
  return path;
}
std::string readBackup(const std::string& data, const EnvironmentPolicy& policy, const std::string& id) {
  const auto bytes = readPrivateFile(backupPath(data, id), 64 * 1024 * 1024 + 1024);
  const auto end = bytes.find('\n');
  if (end == std::string::npos) throw std::runtime_error("invalid backup envelope");
  std::istringstream header(bytes.substr(0, end));
  std::string version, env, instance, economy, backupId, timestamp, format, build, checksum, extra;
  if (!(header >> version >> env >> instance >> economy >> backupId >> timestamp >> format >> build >> checksum) ||
      (header >> extra) || version != "HELION_BACKUP_V1" || env + " " + instance + " " + economy != identity(policy) ||
      backupId != id || timestamp.empty() || timestamp.find_first_not_of("0123456789") != std::string::npos ||
      format != "persistence-admin1" || build != "native-0.2.0" || checksum != digest(std::string_view(bytes).substr(end + 1)))
    throw std::runtime_error("backup integrity/identity mismatch");
  return bytes.substr(end + 1);
}
} // namespace helion::server
