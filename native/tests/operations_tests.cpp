#include "server/operations.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <unistd.h>

void check(bool value, const char* message) {
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
template<class F> void rejected(F action, const char* message) {
  bool failed = false;
  try { action(); } catch (...) { failed = true; }
  check(failed, message);
}
int main() {
  using namespace helion::server;
  ServiceLifecycle lifecycle;
  check(!lifecycle.ready(), "starting is not ready");
  lifecycle.online(); check(lifecycle.ready(), "online is ready");
  lifecycle.maintenance(10, 30);
  check(lifecycle.phase == ServicePhase::maintenance_soon && lifecycle.ready(), "pending remains available");
  lifecycle.maintenance(20, 99); // Repeated signals cannot postpone maintenance.
  check(!lifecycle.advance(39, 5) && lifecycle.ready(), "warning deadline");
  check(!lifecycle.advance(40, 5) && !lifecycle.ready(), "drain stops admission");
  lifecycle.drain(41, 99); // Repeated stop signals cannot postpone shutdown.
  check(!lifecycle.advance(44.99, 5) && lifecycle.advance(45, 5), "bounded drain");
  ServiceLifecycle stopping;
  stopping.online(); stopping.maintenance(0, 60); stopping.drain(2, 1);
  check(stopping.advance(3, 1), "termination skips warning");
  char temp[] = "/tmp/helion-operations-unit-XXXXXX";
  check(mkdtemp(temp) != nullptr, "temporary directory");
  const std::string data = std::string(temp) + "/universe.db";
  EnvironmentPolicy policy;
  policy.environment = RuntimeEnvironment::production; policy.explicitEnvironment = true;
  const std::string content = "E\tproduction\nunit-test-payload\n";
  writePrivateFile(data, content);
  createBackup(data, policy, "baseline");
  check(readBackup(data, policy, "baseline") == content, "backup exact roundtrip");
  rejected([&] { createBackup(data, policy, "baseline"); }, "no backup overwrite");
  check(readBackup(data, policy, "baseline") == content, "duplicate leaves valid backup");
  policy.environment = RuntimeEnvironment::private_test;
  rejected([&] { readBackup(data, policy, "baseline"); }, "cross-environment rejected");
  policy.environment = RuntimeEnvironment::production;
  policy.instance = "other";
  rejected([&] { readBackup(data, policy, "baseline"); }, "cross-instance rejected");
  policy.instance = "local";
  const auto path = std::string(temp) + "/.helion-backups/baseline.snapshot";
  const auto original = readPrivateFile(path, 4096);
  for (const auto& bytes : {std::string("missing-metadata\n"), original.substr(0, original.size() - 3),
                            original + "corrupt", std::string("HELION_TEST_SNAPSHOT_V1 test\n")}) {
    replacePrivateFile(path, bytes);
    rejected([&] { readBackup(data, policy, "baseline"); }, "invalid backup rejected");
  }
  rejected([&] { readBackup(data, policy, "../escape"); }, "no arbitrary backup path");
  replacePrivateFile(path, original);
  check(readBackup(data, policy, "baseline") == content, "verified backup preserved");
  ServiceMetrics metrics;
  metrics.saves = 3; metrics.saveFailures = 1; metrics.backups = 1;
  publishStatus(data, policy, ServicePhase::online, metrics, 10, 2, 1, true);
  const auto status = readPrivateFile(data + ".status.json", 8192);
  check(status.find("\"ready\":true") != std::string::npos && status.find("\"saves_total\":3") != std::string::npos,
        "health and metrics");
  publishStatus(data, policy, ServicePhase::draining, metrics, 11, 2, 1, true);
  check(readPrivateFile(data + ".status.json", 8192).find("\"ready\":false") != std::string::npos, "drain unready");
  std::filesystem::remove_all(temp);
  std::cout << "operations lifecycle, metrics and backup tests passed\n";
}
