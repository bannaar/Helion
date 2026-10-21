#include "server/test_admin.h"
#include <openssl/sha.h>
#include <algorithm>
#include <cerrno>
#include <ctime>
#include <filesystem>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace helion::server {
namespace {
void validateFile(int fd) {
  struct stat st{};
  if (fd < 0 || fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_nlink != 1 ||
      st.st_uid != geteuid() || (st.st_mode & 077) != 0)
    throw std::runtime_error("unsafe private file");
}
void writeAll(int fd, std::string_view content) {
  while (!content.empty()) {
    const auto count = write(fd, content.data(), content.size());
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) throw std::runtime_error("private file write failed");
    content.remove_prefix(static_cast<std::size_t>(count));
  }
  if (fsync(fd)) throw std::runtime_error("private file sync failed");
}
std::string quoted(std::string_view value) {
  // Auditing accepts normalized data only; quoting also prevents log injection.
  std::ostringstream out;
  out << '"';
  for (unsigned char ch : value) {
    if (ch == '"' || ch == '\\') out << '\\' << ch;
    else if (ch >= 32 && ch < 127) out << ch;
    else out << '?';
  }
  out << '"';
  return out.str();
}
}
bool safeIdentifier(std::string_view value, std::size_t maximum) {
  return !value.empty() && value.size() <= maximum && value.front() != '.' &&
    value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") == std::string_view::npos;
}
std::string_view roleName(AccountRole role) {
  switch (role) {
    case AccountRole::normal_player: return "NORMAL_PLAYER";
    case AccountRole::invited_tester: return "INVITED_TESTER";
    case AccountRole::dev_admin: return "DEV_ADMIN";
  }
  return "UNKNOWN";
}
AccountRole EnvironmentPolicy::role(std::string_view account) const {
  const auto found = accounts.find(std::string(account));
  return found == accounts.end() ? AccountRole::normal_player : found->second;
}
bool EnvironmentPolicy::admits(std::string_view account) const {
  if (runtimeEnvironmentName(environment) == "unknown") return false;
  return environment != RuntimeEnvironment::private_test ||
    (explicitEnvironment && accounts.count(std::string(account)) != 0);
}
bool EnvironmentPolicy::cheapEconomy() const {
  return explicitEnvironment && environment == RuntimeEnvironment::private_test && convenienceEconomy;
}
bool EnvironmentPolicy::bypassShipRequirements(std::string_view account) const {
  return explicitEnvironment && environment == RuntimeEnvironment::private_test && admits(account) &&
    (role(account) == AccountRole::dev_admin || role(account) == AccountRole::invited_tester);
}
std::string_view EnvironmentPolicy::releaseChannel() const {
  if (environment == RuntimeEnvironment::private_test && explicitEnvironment) return "TEST";
  if (environment == RuntimeEnvironment::production && explicitEnvironment) return "STABLE";
  return "DEVELOPMENT";
}
std::string readPrivateFile(const std::string& path, std::size_t maximum) {
  const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  try {
    validateFile(fd);
    std::string result;
    char buffer[4096];
    ssize_t count;
    while ((count = read(fd, buffer, sizeof(buffer))) != 0) {
      if (count < 0 && errno == EINTR) continue;
      if (count < 0 || result.size() + static_cast<std::size_t>(count) > maximum)
        throw std::runtime_error("invalid private file size/read");
      result.append(buffer, static_cast<std::size_t>(count));
    }
    close(fd);
    return result;
  } catch (...) { if (fd >= 0) close(fd); throw; }
}
void writePrivateFile(const std::string& path, std::string_view content, bool exclusive) {
  const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK |
    (exclusive ? O_EXCL : O_APPEND), 0600);
  try { validateFile(fd); writeAll(fd, content); close(fd); }
  catch (...) { if (fd >= 0) close(fd); throw; }
}
void EnvironmentPolicy::loadAccessFile(const std::string& path) {
  std::istringstream input(readPrivateFile(path, 65536));
  std::string header, env, id, extra, line;
  if (!std::getline(input, line)) throw std::runtime_error("missing access policy");
  std::istringstream first(line);
  if (!(first >> header >> env >> id) || (first >> extra) || header != "HELION_ACCESS_V1" ||
      env != runtimeEnvironmentName(environment) || !safeIdentifier(id))
    throw std::runtime_error("invalid access policy identity");
  instance = id;
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::string account, roleText, verifier;
    if (!(row >> account >> roleText >> verifier) || (row >> extra) || !safeIdentifier(account, 32) || verifier.size() != 115 ||
        verifier.substr(0, 18) != "$scrypt$32768$8$1$" ||
        verifier.substr(18, 32).find_first_not_of("0123456789abcdef") != std::string::npos ||
        verifier[50] != '$' || verifier.substr(51).find_first_not_of("0123456789abcdef") != std::string::npos)
      throw std::runtime_error("invalid access policy row");
    AccountRole value;
    if (roleText == "DEV_ADMIN") value = AccountRole::dev_admin;
    else if (roleText == "INVITED_TESTER") value = AccountRole::invited_tester;
    else if (roleText == "NORMAL_PLAYER") value = AccountRole::normal_player;
    else throw std::runtime_error("unknown account role");
    invitationCredentials.emplace(account, verifier);
    if (!accounts.emplace(account, value).second) throw std::runtime_error("duplicate account policy");
  }
}
void EnvironmentPolicy::protectStorage(const std::string& data, const std::string& access,
    const std::string& certificate, const std::string& key) const {
  namespace fs = std::filesystem;
  const auto root = fs::canonical(fs::absolute(data).parent_path());
  for (auto parent = root.parent_path(); !parent.empty(); parent = parent.parent_path()) {
    const auto ancestorMarker = parent / ".helion-environment";
    if (fs::exists(ancestorMarker) || fs::is_symlink(ancestorMarker)) {
      std::istringstream markerInput(readPrivateFile(ancestorMarker.string(), 256));
      std::string version, ancestorEnvironment;
      if (!(markerInput >> version >> ancestorEnvironment) || version != "HELION_STORAGE_V1" ||
          ancestorEnvironment != runtimeEnvironmentName(environment))
        throw std::runtime_error("nested cross-environment storage rejected");
    }
    if (parent == parent.root_path()) break;
  }
  const auto marker = root / ".helion-environment";
  const std::string identity = "HELION_STORAGE_V1 " + std::string(runtimeEnvironmentName(environment)) +
    " " + instance + " " + (cheapEconomy() ? "convenience" : "production-like") + "\n";
  if (fs::exists(marker) || fs::is_symlink(marker)) {
    if (readPrivateFile(marker.string(), 256) != identity)
      throw std::runtime_error("storage environment/instance/economy policy mismatch");
  }
  if (environment == RuntimeEnvironment::development) return;
  if (!explicitEnvironment) throw std::runtime_error("explicit server environment required");
  struct stat st{};
  if (stat(root.c_str(), &st) || st.st_uid != geteuid() || (st.st_mode & 077) != 0)
    throw std::runtime_error("environment storage directory must be owner-only");
  for (const auto& file : {access, certificate, key}) {
    if (file.empty()) continue;
    const auto fileRoot = fs::canonical(file).parent_path();
    const auto fileMarker = fileRoot / ".helion-environment";
    if (fileRoot != root && (fs::exists(fileMarker) || fs::is_symlink(fileMarker))) {
      std::istringstream identityFile(readPrivateFile(fileMarker.string(), 256));
      std::string version, fileEnvironment;
      if (!(identityFile >> version >> fileEnvironment) || version != "HELION_STORAGE_V1" ||
          fileEnvironment != runtimeEnvironmentName(environment))
        throw std::runtime_error("cross-environment credential path rejected");
    }
  }
  if (environment == RuntimeEnvironment::private_test) {
    if (access.empty()) throw std::runtime_error("TEST requires a trusted access file");
    for (const auto& file : {access, certificate, key}) {
      if (fs::canonical(file).parent_path() != root || fs::is_symlink(file))
        throw std::runtime_error("TEST requires isolated colocated access/TLS files");
      (void)readPrivateFile(file, 65536);
    }
  }
  if (!fs::exists(marker)) writePrivateFile(marker.string(), identity);
}
std::string AdminOperation::canonical() const {
  return verbText + " " + target + " " + field + " " + value;
}
std::optional<AdminOperation> parseAdminOperation(std::string_view payload) {
  AdminOperation op;
  std::string env, extra;
  std::istringstream input{std::string(payload)};
  if (!(input >> env >> op.id >> op.verbText >> op.target >> op.field >> op.value) ||
      (input >> extra) || env != "TEST" || !safeIdentifier(op.id, 18) || op.id.find_first_not_of("0123456789") != std::string::npos ||
      !safeIdentifier(op.target, 32) || !safeIdentifier(op.field) || !safeIdentifier(op.value)) return std::nullopt;
  const std::map<std::string, AdminVerb> verbs{{"GET", AdminVerb::get}, {"SET", AdminVerb::set},
    {"ADD", AdminVerb::add}, {"REMOVE", AdminVerb::remove}, {"GRANT", AdminVerb::grant},
    {"REVOKE", AdminVerb::revoke}, {"RESET", AdminVerb::reset}, {"TELEPORT", AdminVerb::teleport}};
  const auto verb = verbs.find(op.verbText);
  if (verb == verbs.end()) return std::nullopt;
  op.verb = verb->second;
  // Never audit arbitrary strings as parameters: only allow concrete numeric or
  // registry identifiers here, with registry validation at the domain boundary.
  static const std::string fields[] = {"gsc", "experience", "ore", "food", "parts", "hull", "fuel",
    "salvage", "ship", "module", "station", "player", "markets"};
  if (std::find(std::begin(fields), std::end(fields), op.field) == std::end(fields)) return std::nullopt;
  return op;
}
std::string authorizeTestAdmin(const AuthorizationContext& context) {
  if (!context.policy.explicitEnvironment || context.policy.environment != RuntimeEnvironment::private_test)
    return "test-environment-required";
  if (!context.authenticatedPlayer || context.account.empty()) return "player-authentication-required";
  if (!context.policy.admits(context.account) || context.policy.role(context.account) != AccountRole::dev_admin)
    return "dev-admin-required";
  return {};
}
void appendAudit(const std::string& data, const EnvironmentPolicy& policy, const AuditRecord& r) {
  std::ostringstream accountBytes;
  for (unsigned char byte : r.account)
    accountBytes << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
  std::ostringstream out;
  out << "{\"timestamp\":" << std::time(nullptr) << ",\"environment\":" << quoted(runtimeEnvironmentName(policy.environment))
      << ",\"instance\":" << quoted(policy.instance) << ",\"account\":" << quoted(r.account)
      << ",\"account_id_hex\":" << quoted(accountBytes.str())
      << ",\"role\":" << quoted(roleName(policy.role(r.account))) << ",\"session\":" << quoted(r.session);
  for (const auto& field : {std::pair{"action", r.action}, {"target", r.target}, {"parameters", r.parameters},
       {"before", r.before}, {"requested", r.requested}, {"after", r.after}, {"result", r.result},
       {"reason", r.reason}, {"request", r.request}})
    out << "," << quoted(field.first) << ":" << quoted(field.second);
  out << "}\n";
  writePrivateFile(data + ".audit.jsonl", out.str(), false);
}
std::string digest(std::string_view content) {
  unsigned char bytes[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(content.data()), content.size(), bytes);
  std::ostringstream out;
  for (unsigned char byte : bytes) out << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
  return out.str();
}
} // namespace helion::server
