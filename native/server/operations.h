#pragma once
#include "server/test_admin.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

namespace helion::server {
enum class ServicePhase { starting, validating, loading, online, maintenance_soon, draining, maintenance, degraded, shutting_down, offline, fatal };
std::string_view phaseName(ServicePhase phase);
// Main-thread lifecycle; callers supply monotonic seconds for deterministic tests.
class ServiceLifecycle {
 public:
  ServicePhase phase = ServicePhase::starting;
  void online();
  void maintenance(double now, unsigned warningSeconds);
  void drain(double now, unsigned drainSeconds);
  bool advance(double now, unsigned drainSeconds);
  bool ready() const;
 private:
  double deadline_ = 0;
};
struct ServiceMetrics {
  std::atomic<std::uint64_t> connections{0}, rejected{0}, requests{0}, malformed{0}, authFailures{0};
  std::atomic<std::uint64_t> saves{0}, saveFailures{0}, saveMicros{0}, lastSave{0};
  std::atomic<std::uint64_t> backups{0}, backupFailures{0}, lastBackup{0}, loadMicros{0};
  std::atomic<bool> storageFatal{false}, persistenceHealthy{true};
};
// POSIX service-account trust only. No game/companion authorization or network API.
void operationLog(const EnvironmentPolicy& policy, std::string_view event, std::string_view result);
void syncDirectory(const std::string& path);
void replacePrivateFile(const std::string& path, std::string_view content, bool durable = true);
void publishStatus(const std::string& data, const EnvironmentPolicy& policy, ServicePhase phase,
                   const ServiceMetrics& metrics, std::uint64_t uptime, std::size_t connections,
                   std::size_t players, bool durable);
std::string createBackup(const std::string& data, const EnvironmentPolicy& policy, const std::string& id);
std::string readBackup(const std::string& data, const EnvironmentPolicy& policy, const std::string& id);
std::string uniqueBackupId(std::string_view prefix);
} // namespace helion::server
