#pragma once

#include "server/environment.h"
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace helion::server {
enum class AccountRole { normal_player, invited_tester, dev_admin };
std::string_view roleName(AccountRole role);
struct EnvironmentPolicy {
  RuntimeEnvironment environment = RuntimeEnvironment::development;
  bool explicitEnvironment = false;
  bool convenienceEconomy = true;
  std::string instance = "local";
  std::map<std::string, AccountRole> accounts;
  std::map<std::string, std::string> invitationCredentials;
  AccountRole role(std::string_view account) const;
  bool admits(std::string_view account) const;
  bool cheapEconomy() const;
  bool bypassShipRequirements(std::string_view account) const;
  std::string_view releaseChannel() const;
  void loadAccessFile(const std::string& path);
  // Called before migration/save. TEST requires a dedicated owner-only directory,
  // colocated private credentials and roster. All environments honor its marker.
  void protectStorage(const std::string& data, const std::string& access,
                      const std::string& certificate, const std::string& key) const;
};
enum class AdminVerb { get, set, add, remove, grant, revoke, reset, teleport };
struct AdminOperation {
  std::string id, target, field, value;
  AdminVerb verb = AdminVerb::get;
  std::string verbText;
  std::string canonical() const;
};
bool safeIdentifier(std::string_view value, std::size_t maximum = 48);
std::optional<AdminOperation> parseAdminOperation(std::string_view payload);
struct AuthorizationContext {
  const EnvironmentPolicy& policy;
  std::string account;
  bool authenticatedPlayer = false;
};
std::string authorizeTestAdmin(const AuthorizationContext& context);
struct AuditRecord {
  std::string account, session, action, target, parameters, before, requested, after, result, reason, request;
};
void appendAudit(const std::string& data, const EnvironmentPolicy& policy, const AuditRecord& record);
// Owner-only, no symlinks/hardlinks, bounded reads. Used for trusted config and
// snapshots, never accepts a path from a game-protocol request.
std::string readPrivateFile(const std::string& path, std::size_t maximum);
void writePrivateFile(const std::string& path, std::string_view content, bool exclusive = true);
std::string digest(std::string_view content);
} // namespace helion::server
