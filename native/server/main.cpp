#include <arpa/inet.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <deque>
#include <filesystem>
#include <charconv>
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <iomanip>
#include <memory>
#include <limits>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <poll.h>
#include <sstream>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "shared/protocol.h"
#include "server/security.h"
#include "server/connection_limit.h"
#include "server/data_lock.h"
#include "server/environment.h"
#include "server/test_admin.h"
#include "server/operations.h"
#include "shared/flight.h"
#include "shared/combat.h"
#include "shared/binary_protocol.h"
#include "shared/career.h"
#include "shared/economy.h"
#include "shared/galaxy.h"
#include "shared/loadout.h"
#include "shared/organizations.h"
#include "shared/owned_ship.h"
#include "shared/ships.h"
#include "shared/shipyard.h"
#include "shared/tls.h"

namespace {
constexpr int kDefaultPort = 4242;
constexpr std::size_t kDefaultMaxClients = 32;
constexpr int kReadTimeoutSeconds = 30;
constexpr int kWriteTimeoutSeconds = 5;
constexpr int kMaxAuthFailures = 5;
constexpr int kMaxAuthRequests = 10;
constexpr std::size_t kMaxCompanionTokensPerUser = 8;
constexpr std::int64_t kCompanionTokenLifetimeSeconds = 30LL * 24 * 60 * 60;
constexpr const char* kCompanionProfileReadScope = "profile.read";
constexpr double kFlightCheckpointSeconds = 1.0;
constexpr double kMarketRestockSeconds = 60.0;
std::string dataPath = "helion-server.db";
helion::server::EnvironmentPolicy environmentPolicy;
auto& runtimeEnvironment = environmentPolicy.environment;
std::string accessFile;
struct AdminReceipt { std::string fingerprint, result; };
std::map<std::string, AdminReceipt> adminReceipts;
std::string dummyPasswordHash;
volatile std::sig_atomic_t stopping = 0;
volatile std::sig_atomic_t maintenanceRequested = 0;
helion::server::ServiceMetrics serviceMetrics;
std::atomic<bool> draining{false};

void stopSignal(int) { stopping = 1; }
void maintenanceSignal(int) { maintenanceRequested = 1; }

struct Profile {
  std::string passwordHash;
  std::string display;
  std::string faction = "free-traders";
  int credits = 1500;
  int experience = 0;
  bool legacyCredential = false;
  int missionStage = 0;
  helion::career::State career;
  bool missionOreMined = false;
  std::vector<helion::ships::OwnedShip> ownedShips;
  std::string activeShipId;
  unsigned nextShipSequence = 2;
  bool legacyProjectionOverFuel = false;
  // These combat fields are the persistent reward claim and recovery state;
  // the NPC itself is rebuilt at runtime from this small seed.
  int salvage = 0;
  int combatTargetGeneration = 1;
  bool combatTargetDefeated = false;
  std::map<std::string, int> reputation;
  helion::galaxy::KnowledgeState galaxyKnowledge{};
  std::deque<std::string> combatEvents;
  helion::combat::Hostile hostile{};
};

struct ChatMessage {
  std::string user;
  std::string text;
};

struct GalNetEvent {
  std::string id;
  std::string headline;
};

struct CompanionTokenRecord {
  std::string id;
  std::string user;
  std::string tokenHash;
  std::int64_t expiresAt = 0;
  std::string scope = kCompanionProfileReadScope;
};

enum class AuthKind { none, player, companion };

std::mutex stateMutex;
std::unordered_map<std::string, Profile> profiles;
std::unordered_map<std::string, CompanionTokenRecord> companionTokens;
std::vector<ChatMessage> messages;
std::vector<GalNetEvent> galnetEvents;
std::vector<int> clients;
std::set<int> authenticatedClients;
bool flightStateDirty = false;
bool ownershipMigrationPending = false;
bool environmentMigrationPending = false;
bool economyMigrationPending = true;
bool galaxyMigrationPending = true;
bool knowledgeMigrationPending = true;
helion::economy::MarketState regionalMarkets{};
helion::economy::Telemetry economicTelemetry{};
helion::galaxy::GalaxyState galaxyState{};
std::mutex transportMutex;
struct Transport {
  explicit Transport(std::shared_ptr<helion::tls::Connection> value) : connection(std::move(value)) {}
  std::shared_ptr<helion::tls::Connection> connection;
  std::mutex writeMutex;
  bool binaryMode = false;
};
std::unordered_map<int, std::shared_ptr<Transport>> transports;

helion::economy::PricingPolicy pricingPolicy() {
  return environmentPolicy.cheapEconomy() ?
    helion::economy::PricingPolicy::testConvenience : helion::economy::PricingPolicy::productionLike;
}

int configuredPurchasePrice(int canonicalPrice, bool testAvailable = true) {
  const auto price = helion::server::purchasePrice(environmentPolicy.cheapEconomy() ?
    helion::server::RuntimeEnvironment::private_test : helion::server::RuntimeEnvironment::production, canonicalPrice, testAvailable);
  if (!price) throw std::logic_error("invalid runtime economy policy");
  return *price;
}

std::int64_t creditsInCirculation() {
  std::int64_t total = 0;
  for (const auto& [name, profile] : profiles) {
    (void)name;
    total += profile.credits;
  }
  return total;
}

helion::ships::OwnedShip& activeShip(Profile& profile) {
  const auto found = std::find_if(profile.ownedShips.begin(), profile.ownedShips.end(),
    [&profile](const auto& ship) { return ship.instanceId == profile.activeShipId; });
  if (found == profile.ownedShips.end()) throw std::logic_error("profile has no active ship");
  return *found;
}

const helion::ships::OwnedShip& activeShip(const Profile& profile) {
  const auto found = std::find_if(profile.ownedShips.begin(), profile.ownedShips.end(),
    [&profile](const auto& ship) { return ship.instanceId == profile.activeShipId; });
  if (found == profile.ownedShips.end()) throw std::logic_error("profile has no active ship");
  return *found;
}

void addStarterShip(Profile& profile, std::string_view commander, const helion::flight::State& legacyFlight = {},
                    int hullLevel = 1, int engineLevel = 1,
                    std::vector<std::string> ownedModules = {"mining-basic", "engine-basic", "hull-standard"},
                    std::array<std::string, helion::loadout::kSlotCount> fittedModules = {{
                      "mining-basic", "engine-basic", "hull-standard", ""}}) {
  auto ship = helion::ships::makeStarterShip(commander, legacyFlight, hullLevel, engineLevel);
  ship.ownedModules = std::move(ownedModules);
  ship.fittedModules = std::move(fittedModules);
  profile.activeShipId = ship.instanceId;
  profile.ownedShips = {std::move(ship)};
  profile.nextShipSequence = 2;
}

const helion::loadout::ModuleDefinition* fittedModule(const Profile& profile, helion::loadout::Slot slot) {
  const auto& id = activeShip(profile).fittedModules[helion::loadout::slotIndex(slot)];
  return id.empty() ? nullptr : helion::loadout::find(id);
}

bool ownsModule(const Profile& profile, std::string_view id) {
  const auto& modules = activeShip(profile).ownedModules;
  return std::find(modules.begin(), modules.end(), id) != modules.end();
}

std::string joinModules(const std::vector<std::string>& modules) {
  std::string result;
  for (const auto& module : modules) {
    if (!result.empty()) result += ',';
    result += module;
  }
  return result;
}

void initializeReputation(Profile& profile) {
  for (const auto& definition : helion::organizations::kRegistry)
    profile.reputation.emplace(definition.id, 0);
}

int standing(const Profile& profile, std::string_view id) {
  const auto found = profile.reputation.find(std::string(id));
  return found == profile.reputation.end() ? 0 : found->second;
}

void changeStanding(Profile& profile, std::string_view id, int delta) {
  if (!helion::organizations::find(id)) throw std::runtime_error("unknown organization");
  initializeReputation(profile);
  auto& value = profile.reputation[std::string(id)];
  value = helion::organizations::clampStanding(value + delta);
}

std::string reputationLine(const Profile& profile, std::string_view id, int delta = 0) {
  const auto* definition = helion::organizations::find(id);
  const int value = standing(profile, id);
  return "REPUTATION CHANGE id=" + std::string(id) + " display=" + std::string(definition ? definition->wireName : "Unknown") +
    " delta=" + std::to_string(delta) + " value=" + std::to_string(value) +
    " standing=" + helion::organizations::standingLabel(value);
}

bool addGalNetEvent(std::string_view id, std::string_view headline) {
  for (const auto& event : galnetEvents) if (event.id == id) return false;
  if (id.empty() || id.size() > 64 || headline.empty() || headline.size() > 512 ||
      headline.find_first_of("\t\r\n") != std::string_view::npos) throw std::runtime_error("malformed GalNet event");
  galnetEvents.push_back({std::string(id), std::string(headline)});
  return true;
}

void refreshDerivedShipState(Profile& profile) {
  auto& ship = activeShip(profile);
  const auto* defense = fittedModule(profile, helion::loadout::Slot::defense);
  helion::ships::refreshDerivedState(ship, defense ? defense->hullBonus : 0);
}

void resetCombatTarget(Profile& profile) {
  helion::combat::resetHostile(profile.hostile, profile.combatTargetGeneration);
  profile.hostile.destroyed = profile.combatTargetDefeated;
}

std::string loadoutLine(const Profile& profile) {
  const auto& ship = activeShip(profile);
  const auto fitted = [&ship](helion::loadout::Slot slot) {
    const auto& id = ship.fittedModules[helion::loadout::slotIndex(slot)];
    return id.empty() ? std::string("none") : id;
  };
  return "LOADOUT owned=" + joinModules(ship.ownedModules) +
    " fitted-mining=" + fitted(helion::loadout::Slot::mining) +
    " fitted-engine=" + fitted(helion::loadout::Slot::engine) +
    " fitted-defense=" + fitted(helion::loadout::Slot::defense) +
    " fitted-weapon=" + fitted(helion::loadout::Slot::weapon);
}

std::string wireToken(std::string_view value) {
  std::string result(value);
  std::replace(result.begin(), result.end(), ' ', '_');
  return result;
}

std::string moduleDefinitionLine(const Profile& profile, const helion::loadout::ModuleDefinition& module) {
  const auto fit = helion::loadout::compatibility(module, helion::ships::definition(activeShip(profile)));
  const auto& fitted = activeShip(profile).fittedModules[helion::loadout::slotIndex(module.slot)];
  return "MODULE id=" + std::string(module.id) + " name=" + wireToken(module.displayName) +
    " manufacturer=" + module.manufacturerId + " category=" + helion::loadout::categoryName(module.category) +
    " slot=" + helion::loadout::slotName(module.slot) +
    " slot-type=" + helion::loadout::slotTypeName(module.slotType) +
    " size=" + helion::ships::slotSizeName(module.size) + " grade=" + std::string(1, module.grade) +
    " price=" + std::to_string(configuredPurchasePrice(module.purchasePrice, module.testAvailable)) +
    " canonical-price=" + std::to_string(module.purchasePrice) + " mass=" + std::to_string(module.mass) +
    " power=" + std::to_string(module.powerDraw) + " integrity=" + std::to_string(module.integrity) +
    " legal=" + helion::loadout::legalStatusName(module.legalStatus) +
    " effect=" + helion::loadout::effectSummary(module) +
    " test-available=" + std::to_string(module.testAvailable) +
    " owned=" + std::to_string(ownsModule(profile, module.id)) +
    " fitted=" + std::to_string(fitted == module.id) +
    " compatible=" + std::to_string(fit.compatible) +
    " reason=" + helion::loadout::fitIssueName(fit.issue);
}

template <typename ResponseContainer>
void appendOutfittingState(ResponseContainer& responses, const Profile& profile) {
  for (const auto& module : helion::loadout::kCatalogue)
    responses.push_back(moduleDefinitionLine(profile, module));
  responses.push_back(loadoutLine(profile));
}

std::string shipDefinitionLine(const helion::ships::Definition& hull) {
  std::string hardpoints;
  for (const auto& hardpoint : hull.hardpoints) {
    if (!hardpoints.empty()) hardpoints += ',';
    hardpoints += helion::ships::slotSizeName(hardpoint.size);
    hardpoints += ':';
    hardpoints += hardpoint.mount;
  }
  std::string tags;
  for (const auto tag : hull.visualTags) {
    if (!tags.empty()) tags += ',';
    tags += tag;
  }
  const auto* manufacturer = helion::ships::findManufacturer(hull.manufacturerId);
  const auto* operatorProfile = helion::ships::findOperator(hull.operatorId);
  return "SHIPDEF id=" + std::string(hull.id) + " manufacturer=" + wireToken(manufacturer->displayName) +
    " manufacturer-id=" + std::string(hull.manufacturerId) + " operator=" + wireToken(operatorProfile->displayName) +
    " operator-id=" + std::string(hull.operatorId) + " model=" + wireToken(hull.model) +
    " name=" + wireToken(hull.displayName) + " class=" + wireToken(hull.shipClass) +
    " role=" + wireToken(hull.role) + " pad=" + helion::ships::padSizeName(hull.padSize) +
    " price=" + std::to_string(configuredPurchasePrice(hull.purchasePrice)) +
    " canonical-price=" + std::to_string(hull.purchasePrice) +
    " mass=" + std::to_string(static_cast<int>(hull.mass)) +
    " speed=" + std::to_string(static_cast<int>(hull.topSpeed)) +
    " boost=" + std::to_string(static_cast<int>(hull.boostSpeed)) +
    " acceleration=" + std::to_string(static_cast<int>(hull.acceleration)) +
    " handling=" + std::to_string(hull.maneuverability) + " hull=" + std::to_string(hull.baseHull) +
    " shields=" + std::to_string(hull.baseShields) + " cargo=" + std::to_string(hull.cargoCapacity) +
    " fuel=" + std::to_string(static_cast<int>(hull.fuelCapacity)) +
    " jump-laden=" + std::to_string(hull.jumpRangeLaden) +
    " jump-unladen=" + std::to_string(hull.jumpRangeUnladen) + " hardpoints=" + hardpoints +
    " utilities=" + std::to_string(hull.utilityMounts.size()) + " hull-family=" + std::string(hull.hullFamily) +
    " silhouette=" + std::string(hull.silhouetteFamily) + " tags=" + tags;
}

std::string ownedShipLine(const Profile& profile, const helion::ships::OwnedShip& ship) {
  const auto* variant = helion::ships::findVisualVariant(ship.visualVariantId);
  return "OWNEDSHIP instance=" + ship.instanceId + " hull=" + ship.hullId +
    " name=" + wireToken(ship.customName) + " station=" + std::to_string(ship.flight.station) +
    " state=" + (ship.instanceId == profile.activeShipId ? "ACTIVE" : "STORED") +
    " hull-condition=" + std::to_string(ship.flight.hull) + " max-hull=" + std::to_string(ship.flight.maxHull) +
    " fuel=" + std::to_string(static_cast<int>(ship.flight.fuel)) +
    " cargo=" + std::to_string(helion::flight::cargoUsed(ship.flight)) +
    " cargo-capacity=" + std::to_string(helion::ships::cargoCapacity(ship)) +
    " variant=" + ship.visualVariantId + " operator=" + wireToken(
      helion::ships::findOperator(variant->operatorId)->displayName) +
    " livery=" + ship.livery + " wear=" + ship.wearState;
}

template <typename ResponseContainer>
void appendShipyardState(ResponseContainer& responses, const Profile& profile, bool includeOffers = true, bool testBypass = false) {
  const auto& current = activeShip(profile);
  const int station = current.flight.station;
  responses.push_back("SHIPYARD BEGIN station=" + std::to_string(station));
  if (includeOffers) {
    for (const auto* hull : helion::shipyard::availableAt(station,
      helion::flight::kStations[static_cast<std::size_t>(station)].maximumPad, profile.reputation, testBypass)) {
      if (std::any_of(profile.ownedShips.begin(), profile.ownedShips.end(),
          [hull](const auto& owned) { return owned.hullId == hull->id; })) continue;
      responses.push_back(shipDefinitionLine(*hull));
    }
  }
  for (const auto& owned : profile.ownedShips) responses.push_back(ownedShipLine(profile, owned));
  responses.push_back("SHIPYARD END station=" + std::to_string(station) + " active=" + profile.activeShipId +
    " maximum-pad=" + helion::ships::padSizeName(helion::flight::kStations[static_cast<std::size_t>(station)].maximumPad));
}

std::string encode(const std::string& value) {
  std::string out;
  for (const char ch : value) {
    if (ch == '\\' || ch == '\t' || ch == '\n') out.push_back('\\');
    if (ch == '\t') out.push_back('t');
    else if (ch == '\n') out.push_back('n');
    else out.push_back(ch);
  }
  return out;
}

std::string decode(const std::string& value) {
  std::string out;
  bool escaped = false;
  for (const char ch : value) {
    if (escaped) {
      out.push_back(ch == 't' ? '\t' : ch == 'n' ? '\n' : ch);
      escaped = false;
    } else if (ch == '\\') escaped = true;
    else out.push_back(ch);
  }
  if (escaped) out.push_back('\\');
  return out;
}

void saveStateLocked() {
  const auto saveStart = std::chrono::steady_clock::now();
  bool committed = false;
  struct SaveObservation {
    std::chrono::steady_clock::time_point start;
    bool& committed;
    ~SaveObservation() {
      serviceMetrics.saveMicros += std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count();
      serviceMetrics.persistenceHealthy = committed && !serviceMetrics.storageFatal;
      if (committed) { ++serviceMetrics.saves; serviceMetrics.lastSave = std::time(nullptr); }
      else ++serviceMetrics.saveFailures;
      // Count every save; sample successful logs to avoid a hot-path log flood.
      if (!committed || serviceMetrics.saves == 1 || serviceMetrics.saves % 60 == 0)
        helion::server::operationLog(environmentPolicy, "persistence-save", committed ? "committed" : "failed");
    }
  } observation{saveStart, committed};
  std::ostringstream snapshot;
  snapshot << std::setprecision(17);
  snapshot << "E\t" << helion::server::runtimeEnvironmentName(runtimeEnvironment) << '\n';
  snapshot << "V\tGALAXY_KNOWLEDGE\t1\n";
  snapshot << "L\tTEST_ADMIN\t1\t" << adminReceipts.size() << '\n';
  for (const auto& [key, receipt] : adminReceipts)
    snapshot << "A\t" << key << '\t' << receipt.fingerprint << '\t' << encode(receipt.result) << '\n';
  for (const auto& system : galaxyState)
    snapshot << "Y\t" << system.systemId << '\t' << system.securitySteps << '\t'
      << helion::galaxy::sovereignTypeName(system.sovereignty.type) << '\t'
      << encode(system.sovereignty.id) << '\t'
      << helion::galaxy::discoveryStateName(system.discovery) << '\n';
  for (const auto& market : regionalMarkets)
    snapshot << "K\t" << market.station << '\t' << market.commodityId << '\t' << market.stock << '\t'
      << market.demand << '\t' << market.buyBudget << '\n';
  snapshot << "T\t" << helion::economy::serializeTelemetry(economicTelemetry) << '\n';
  std::vector<std::string> commanderNames;
  commanderNames.reserve(profiles.size());
  for (const auto& entry : profiles) commanderNames.push_back(entry.first);
  std::sort(commanderNames.begin(), commanderNames.end());
  for (const auto& name : commanderNames) {
    const auto& profile = profiles.at(name);
    if (!helion::galaxy::validateKnowledge(profile.galaxyKnowledge))
      throw std::runtime_error("invalid commander galaxy knowledge");
    const auto& ship = activeShip(profile);
    const auto& flight = ship.flight;
    snapshot << "H\t" << encode(name) << '\t' << encode(profile.passwordHash) << '\t'
         << encode(profile.display) << '\t' << encode(profile.faction) << '\t'
         << encode(ship.hullId == "SIDEWINDER" ? "sidewinder" : ship.hullId) << '\t' << profile.credits << '\t'
         << profile.experience << '\t' << profile.missionStage << '\t' << ship.hullLevel << '\t'
         << ship.engineLevel << '\t' << flight.x << '\t' << flight.y << '\t'
         << flight.vx << '\t' << flight.vy << '\t' << flight.yaw << '\t'
         << flight.docked << '\t' << flight.cargo << '\t' << flight.food << '\t'
         << flight.parts << '\t' << flight.station << '\t' << flight.hull << '\t'
         << flight.maxHull << '\t' << profile.missionOreMined << '\t' << flight.fuel << '\t'
         << joinModules(ship.ownedModules);
    for (const auto& fitted : ship.fittedModules) snapshot << '\t' << fitted;
    snapshot << '\t' << profile.salvage << '\t' << profile.combatTargetGeneration
             << '\t' << profile.combatTargetDefeated << '\t' << flight.destroyed
             << '\t' << flight.weaponCooldown << '\t'
             << encode(helion::organizations::reputationSummary(profile.reputation)) << '\t'
             << helion::career::serialize(profile.career);
    snapshot << '\n';
    for (const auto& owned : profile.ownedShips) {
      const auto& state = owned.flight;
      snapshot << "S\t" << encode(name) << '\t' << encode(owned.instanceId) << '\t' << owned.hullId << '\t'
        << encode(owned.customName) << '\t' << state.x << '\t' << state.y << '\t' << state.vx << '\t'
        << state.vy << '\t' << state.yaw << '\t' << state.docked << '\t' << state.cargo << '\t'
        << state.food << '\t' << state.parts << '\t' << state.station << '\t' << state.hull << '\t'
        << state.maxHull << '\t' << state.fuel << '\t' << state.maxFuel << '\t' << state.destroyed << '\t'
        << state.weaponCooldown << '\t' << owned.hullLevel << '\t' << owned.engineLevel << '\t'
        << joinModules(owned.ownedModules);
      for (const auto& fitted : owned.fittedModules) snapshot << '\t' << fitted;
      snapshot << '\t' << encode(owned.livery) << '\t' << owned.wearState << '\t' << owned.stored
        << '\t' << (owned.instanceId == profile.activeShipId) << '\t' << owned.visualVariantId << '\n';
    }
    for (const auto& knowledge : profile.galaxyKnowledge)
      snapshot << "D\t" << encode(name) << '\t' << knowledge.systemId << '\t'
        << helion::galaxy::knowledgeLevelName(knowledge.level) << '\n';
  }
  for (const auto& event : galnetEvents)
    snapshot << "G\t" << encode(event.id) << '\t' << encode(event.headline) << '\n';
  const auto now = static_cast<std::int64_t>(std::time(nullptr));
  for (const auto& [id, token] : companionTokens) {
    if (token.expiresAt <= now) continue;
    snapshot << "T\t" << encode(id) << '\t' << encode(token.user) << '\t'
         << encode(token.tokenHash) << '\t' << token.expiresAt << '\t'
         << encode(token.scope) << '\n';
  }
  for (const auto& message : messages) {
    snapshot << "M\t" << encode(message.user) << '\t' << encode(message.text) << '\n';
  }
  const std::string records = snapshot.str();
  snapshot << "Z\tGALAXY_END\t" << galaxyState.size() << '\t' << profiles.size() << '\t'
    << std::count(records.begin(), records.end(), '\n') << '\n';
  std::string temp = dataPath + ".tmp.XXXXXX";
  const int fd = mkstemp(temp.data());
  if (fd < 0) throw std::runtime_error("cannot create persistence file");
  bool complete = false;
  try {
    if (fchmod(fd, S_IRUSR | S_IWUSR) != 0) throw std::runtime_error("cannot protect persistence file");
    const std::string content = snapshot.str();
    std::size_t written = 0;
    while (written < content.size()) {
      const ssize_t count = write(fd, content.data() + written, content.size() - written);
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) throw std::runtime_error("cannot write persistence file");
      written += static_cast<std::size_t>(count);
    }
    if (fsync(fd) != 0) throw std::runtime_error("cannot sync persistence file");
    if (close(fd) != 0) throw std::runtime_error("cannot close persistence file");
    complete = true;
    if (std::rename(temp.c_str(), dataPath.c_str()) != 0) throw std::runtime_error("cannot replace persistence file");
    committed = true;
    // After rename, never roll back only memory on an uncertain directory sync.
    // Fail readiness and terminate service instead of claiming durable success.
    try { helion::server::syncDirectory(dataPath); }
    catch (...) {
      serviceMetrics.storageFatal = true;
      ++serviceMetrics.saveFailures;
      helion::server::operationLog(environmentPolicy, "persistence-durability", "fatal");
    }
  } catch (...) {
    if (!complete) close(fd);
    unlink(temp.c_str());
    throw;
  }
}

void persistStateLocked() {
  saveStateLocked();
  flightStateDirty = false;
}

void loadState() {
  const int fd = open(dataPath.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) {
    if (errno == ENOENT) return;
    throw std::runtime_error("cannot open persistence file");
  }
  struct stat info{};
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1 || info.st_uid != geteuid() ||
      fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
    close(fd);
    throw std::runtime_error("cannot protect persistence file");
  }
  if (runtimeEnvironment == helion::server::RuntimeEnvironment::production && info.st_size == 0) {
    close(fd); throw std::runtime_error("empty existing Production persistence rejected");
  }
  FILE* file = fdopen(fd, "r");
  if (!file) { close(fd); throw std::runtime_error("cannot read persistence file"); }
  char* buffer = nullptr;
  std::size_t capacity = 0;
  std::set<std::string> profilesWithShipRecords;
  std::set<std::string> instanceIds;
  std::set<std::string> marketRecords;
  std::set<std::string> galaxyRecords;
  std::map<std::string, std::set<std::string>> knowledgeRecords;
  bool telemetryRecord = false;
  bool knowledgeVersionRecord = false;
  std::string galaxyEndRecord;
  std::size_t persistenceRecords = 0;
  bool firstRecord = true;
  std::optional<std::size_t> receiptCount;
  try {
    ssize_t length;
    while ((length = ::getline(&buffer, &capacity, file)) >= 0) {
      std::string line(buffer, static_cast<std::size_t>(length));
      if (!line.empty() && line.back() == '\n') line.pop_back();
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (!galaxyEndRecord.empty()) throw std::runtime_error("records after persistence terminator");
      ++persistenceRecords;
      std::istringstream input(line);
      std::string kind, first, second, third;
      std::getline(input, kind, '\t');
      std::getline(input, first, '\t');
      std::getline(input, second, '\t');
      std::getline(input, third, '\t');
      if (firstRecord) {
        firstRecord = false;
        if (kind == "E") {
          const auto persistedEnvironment = helion::server::parseRuntimeEnvironment(first);
          if (!persistedEnvironment || line != "E\t" + first)
            throw std::runtime_error("malformed persistence environment");
          if (*persistedEnvironment != runtimeEnvironment)
            throw std::runtime_error("persistence environment mismatch: expected " +
              std::string(helion::server::runtimeEnvironmentName(runtimeEnvironment)) + ", found " +
              std::string(helion::server::runtimeEnvironmentName(*persistedEnvironment)));
          continue;
        }
        if (runtimeEnvironment != helion::server::RuntimeEnvironment::development)
          throw std::runtime_error("untagged legacy persistence is development-only");
        environmentMigrationPending = true;
      } else if (kind == "E") {
        throw std::runtime_error("persistence environment record must be first and unique");
      }
      if (kind == "L") {
        if (receiptCount || first != "TEST_ADMIN" || second != "1" || third.empty() ||
            std::count(line.begin(), line.end(), '\t') != 3 || third.find_first_not_of("0123456789") != std::string::npos)
          throw std::runtime_error("invalid admin ledger version");
        receiptCount = std::stoul(third);
        if (*receiptCount > 4096) throw std::runtime_error("admin ledger capacity exceeded");
      } else if (kind == "A") {
        if (!receiptCount || first.size() != 64 || second.size() != 64 || third.size() > 256 ||
            first.find_first_not_of("0123456789abcdef") != std::string::npos ||
            second.find_first_not_of("0123456789abcdef") != std::string::npos ||
            !adminReceipts.emplace(first, AdminReceipt{second, decode(third)}).second ||
            std::count(line.begin(), line.end(), '\t') != 3)
          throw std::runtime_error("invalid admin receipt");
      } else if (kind == "Z") {
        if (!knowledgeVersionRecord) throw std::runtime_error("unexpected persistence terminator");
        galaxyEndRecord = line;
      } else if (kind == "V") {
        if (knowledgeVersionRecord || line != "V\tGALAXY_KNOWLEDGE\t1")
          throw std::runtime_error("invalid or duplicate knowledge schema version");
        knowledgeVersionRecord = true;
      } else if (kind == "Y" && !first.empty() && !second.empty() && !third.empty()) {
        std::string sovereignId, discoveryText, extra;
        std::getline(input, sovereignId, '\t');
        std::getline(input, discoveryText, '\t');
        std::getline(input, extra, '\t');
        std::size_t used = 0;
        const long securityValue = std::stol(second, &used);
        const auto sovereignType = helion::galaxy::parseSovereignType(third);
        const auto discovery = helion::galaxy::parseDiscoveryState(discoveryText);
        auto* system = helion::galaxy::findState(galaxyState, first);
        if (std::count(line.begin(), line.end(), '\t') != 5 ||
            used != second.size() || !extra.empty() || !sovereignType || !discovery || !system ||
            !galaxyRecords.emplace(first).second || securityValue < std::numeric_limits<int>::min() ||
            securityValue > std::numeric_limits<int>::max())
          throw std::runtime_error("malformed or duplicate galaxy state");
        system->securitySteps = static_cast<int>(securityValue);
        system->sovereignty = {*sovereignType, decode(sovereignId)};
        system->discovery = *discovery;
      } else if (kind == "K" && !first.empty() && !second.empty() && !third.empty()) {
        std::string demandText, budgetText, extra;
        std::getline(input, demandText, '\t');
        std::getline(input, budgetText, '\t');
        std::getline(input, extra, '\t');
        auto parseInteger = [](const std::string& value) {
          std::size_t used = 0;
          const long long result = std::stoll(value, &used);
          if (used != value.size()) throw std::runtime_error("malformed market number");
          return result;
        };
        const long long stationValue = parseInteger(first);
        const long long stockValue = parseInteger(third);
        const long long demandValue = parseInteger(demandText);
        const long long budgetValue = parseInteger(budgetText);
        if (!extra.empty() || stationValue < 0 || stationValue > std::numeric_limits<int>::max() ||
            stockValue < 0 || stockValue > std::numeric_limits<int>::max() || demandValue < 0 ||
            demandValue > std::numeric_limits<int>::max() || budgetValue < 0)
          throw std::runtime_error("malformed market state");
        const std::string key = first + ":" + second;
        auto* market = helion::economy::findOrder(regionalMarkets, static_cast<int>(stationValue), second);
        if (!market || !marketRecords.emplace(key).second) throw std::runtime_error("unknown or duplicate market state");
        market->stock = static_cast<int>(stockValue);
        market->demand = static_cast<int>(demandValue);
        market->buyBudget = budgetValue;
      } else if (kind == "T" && !first.empty() && second.empty() && third.empty()) {
        // Economic telemetry has exactly one field; companion T rows have five.
        if (std::count(line.begin(), line.end(), '\t') != 1)
          throw std::runtime_error("malformed economic telemetry");
        if (telemetryRecord) throw std::runtime_error("duplicate economic telemetry");
        const auto parsed = helion::economy::parseTelemetry(first);
        if (!parsed) throw std::runtime_error("malformed economic telemetry");
        economicTelemetry = *parsed;
        telemetryRecord = true;
      } else if ((kind == "P" || kind == "H") && !first.empty() && !second.empty() && !third.empty()) {
        std::string faction, ship, creditsText, experienceText;
        std::getline(input, faction, '\t');
        std::getline(input, ship, '\t');
        std::getline(input, creditsText, '\t');
        std::getline(input, experienceText, '\t');
        Profile profile;
        profile.galaxyKnowledge = helion::galaxy::seedKnowledge(galaxyState);
        helion::flight::State legacyFlight;
        int legacyHullLevel = 1;
        int legacyEngineLevel = 1;
        std::vector<std::string> legacyOwnedModules{"mining-basic", "engine-basic", "hull-standard"};
        std::array<std::string, helion::loadout::kSlotCount> legacyFittedModules{{
          "mining-basic", "engine-basic", "hull-standard", ""}};
        initializeReputation(profile);
        profile.passwordHash = decode(second);
        profile.display = decode(third);
        profile.legacyCredential = kind == "P";
        if (!profile.legacyCredential && !helion::security::isEncodedHash(profile.passwordHash))
          throw std::runtime_error("malformed password hash record");
        if (!faction.empty()) profile.faction = decode(faction);
        const std::string legacyHullId = decode(ship);
        if (!legacyHullId.empty() && legacyHullId != "sidewinder" && legacyHullId != "SIDEWINDER" &&
            !helion::ships::find(legacyHullId)) throw std::runtime_error("unknown legacy ship");
        auto parseNumber = [](const std::string& value, int fallback) {
          if (value.empty()) return fallback;
          std::size_t used = 0;
          const int result = std::stoi(value, &used);
          if (used != value.size()) throw std::runtime_error("malformed persistence number");
          return result;
        };
        auto parseDouble = [](const std::string& value, double fallback) {
          if (value.empty()) return fallback;
          std::size_t used = 0;
          const double result = std::stod(value, &used);
          if (used != value.size() || !std::isfinite(result))
            throw std::runtime_error("malformed persistence number");
          return result;
        };
        profile.credits = parseNumber(creditsText, profile.credits);
        profile.experience = parseNumber(experienceText, profile.experience);
        if (profile.credits < 0 || profile.experience < 0)
          throw std::runtime_error("malformed persistence number");
        std::string value;
        std::getline(input, value, '\t'); profile.missionStage = parseNumber(value, 0);
        std::getline(input, value, '\t'); legacyHullLevel = std::clamp(parseNumber(value, 1), 1, 5);
        std::getline(input, value, '\t'); legacyEngineLevel = std::clamp(parseNumber(value, 1), 1, 5);
        std::getline(input, value, '\t'); legacyFlight.x = parseDouble(value, 0);
        std::getline(input, value, '\t'); legacyFlight.y = parseDouble(value, 0);
        std::getline(input, value, '\t'); legacyFlight.vx = parseDouble(value, 0);
        std::getline(input, value, '\t'); legacyFlight.vy = parseDouble(value, 0);
        std::getline(input, value, '\t'); legacyFlight.yaw = parseDouble(value, 0);
        std::getline(input, value, '\t'); legacyFlight.docked = value.empty() || value == "1";
        std::getline(input, value, '\t'); legacyFlight.cargo = parseNumber(value, 0);
        std::getline(input, value, '\t'); legacyFlight.food = parseNumber(value, 0);
        std::getline(input, value, '\t'); legacyFlight.parts = parseNumber(value, 0);
        std::getline(input, value, '\t'); legacyFlight.station = parseNumber(value, 0);
        std::getline(input, value, '\t'); legacyFlight.hull = std::clamp(parseNumber(value, 100), 0, 1000);
        std::getline(input, value, '\t'); legacyFlight.maxHull = parseNumber(value, 100);
        std::vector<std::string> extensions;
        while (std::getline(input, value, '\t')) extensions.push_back(value);
        if (extensions.size() > 14) throw std::runtime_error("unexpected persistence fields");
        if (!extensions.empty()) {
          if (extensions[0] != "0" && extensions[0] != "1")
            throw std::runtime_error("malformed mission objective");
          profile.missionOreMined = extensions[0] == "1";
        }
        const bool hasSavedFuel = extensions.size() >= 2;
        if (hasSavedFuel) legacyFlight.fuel = parseDouble(extensions[1], -1);
        else legacyFlight.fuel = helion::flight::fuelCapacity(legacyEngineLevel);
        if (extensions.size() >= 3) {
          legacyOwnedModules.clear();
          legacyFittedModules.fill("");
          std::size_t start = 0;
          while (start <= extensions[2].size()) {
            const auto end = extensions[2].find(',', start);
            const std::string id = extensions[2].substr(start, end == std::string::npos ? std::string::npos : end - start);
            const auto* module = helion::loadout::find(id);
            if (!module || id.empty() || std::find(legacyOwnedModules.begin(), legacyOwnedModules.end(), id) != legacyOwnedModules.end())
              throw std::runtime_error("malformed persisted module ownership");
            legacyOwnedModules.push_back(id);
            if (end == std::string::npos) break;
            start = end + 1;
          }
          if (legacyOwnedModules.empty()) throw std::runtime_error("malformed persisted module ownership");
        }
        for (std::size_t slot = 0; slot < helion::loadout::kSlotCount && extensions.size() >= slot + 4; ++slot) {
          const std::string& id = extensions[slot + 3];
          if (id.empty()) {
            legacyFittedModules[slot].clear();
            continue;
          }
          const auto* module = helion::loadout::find(id);
          if (!module || helion::loadout::slotIndex(module->slot) != slot ||
              std::find(legacyOwnedModules.begin(), legacyOwnedModules.end(), id) == legacyOwnedModules.end())
            throw std::runtime_error("malformed persisted module fitting");
          legacyFittedModules[slot] = id;
        }
        if (extensions.size() >= 8) profile.salvage = parseNumber(extensions[7], 0);
        if (extensions.size() >= 9) profile.combatTargetGeneration = parseNumber(extensions[8], 1);
        if (extensions.size() >= 10) {
          if (extensions[9] != "0" && extensions[9] != "1") throw std::runtime_error("malformed combat target state");
          profile.combatTargetDefeated = extensions[9] == "1";
        }
        if (extensions.size() >= 11) {
          if (extensions[10] != "0" && extensions[10] != "1") throw std::runtime_error("malformed destruction state");
          legacyFlight.destroyed = extensions[10] == "1";
        }
        if (extensions.size() >= 12) legacyFlight.weaponCooldown = parseDouble(extensions[11], 0);
        if (extensions.size() >= 13 &&
            !helion::organizations::parseReputationSummary(decode(extensions[12]), profile.reputation))
          throw std::runtime_error("malformed persisted reputation");
        profile.career.introDismissed = profile.missionStage != 0;
        if ((extensions.size() >= 14 && !helion::career::parse(extensions[13], profile.missionStage, profile.career)) ||
            !helion::career::valid(profile.career, profile.missionStage))
          throw std::runtime_error("malformed persisted career state");
        if (profile.missionStage == 2) profile.missionOreMined = false;
        if (legacyFlight.cargo < 0 || legacyFlight.food < 0 || legacyFlight.parts < 0 ||
            static_cast<long long>(legacyFlight.cargo) + legacyFlight.food + legacyFlight.parts >
              helion::flight::kMaximumSupportedCargo ||
            legacyFlight.station < 0 || legacyFlight.station >= static_cast<int>(helion::flight::kStations.size()) ||
            legacyFlight.fuel < 0 || profile.salvage < 0 || profile.salvage > 1000000000 ||
            profile.combatTargetGeneration < 1 || profile.combatTargetGeneration > 1000000000 ||
            legacyFlight.weaponCooldown < 0 || legacyFlight.weaponCooldown > 10) throw std::runtime_error("malformed persisted ship state");
        profile.legacyProjectionOverFuel = legacyFlight.fuel > helion::flight::fuelCapacity(legacyEngineLevel);
        const std::string commander = decode(first);
        addStarterShip(profile, commander, legacyFlight, legacyHullLevel, legacyEngineLevel,
                       std::move(legacyOwnedModules), std::move(legacyFittedModules));
        refreshDerivedShipState(profile);
        auto& migratedShip = activeShip(profile);
        // An H row is only a compatibility projection when S rows follow. Its
        // fuel/hull may exceed Sidewinder limits; validate the authoritative S
        // instance instead, or the migrated starter at end of file.
        if (migratedShip.flight.destroyed) {
          migratedShip.flight.hull = 0;
          migratedShip.flight.cargo = 0;
          migratedShip.flight.docked = false;
        }
        migratedShip.flight.inputAge = 1;
        resetCombatTarget(profile);
        if (!profiles.emplace(commander, std::move(profile)).second)
          throw std::runtime_error("duplicate profile in persistence file");
      } else if (kind == "S" && !first.empty() && !second.empty() && !third.empty()) {
        const std::string commander = decode(first);
        const auto profileIt = profiles.find(commander);
        if (profileIt == profiles.end()) throw std::runtime_error("ship record precedes commander");
        auto parseNumber = [](const std::string& value) {
          std::size_t used = 0;
          const int result = std::stoi(value, &used);
          if (used != value.size()) throw std::runtime_error("malformed ship number");
          return result;
        };
        auto parseDouble = [](const std::string& value) {
          std::size_t used = 0;
          const double result = std::stod(value, &used);
          if (used != value.size() || !std::isfinite(result)) throw std::runtime_error("malformed ship number");
          return result;
        };
        std::vector<std::string> fields;
        std::string value;
        while (std::getline(input, value, '\t')) fields.push_back(value);
        if (fields.size() != 28 && fields.size() != 29)
          throw std::runtime_error("unexpected owned ship fields");
        auto& profile = profileIt->second;
        if (profilesWithShipRecords.emplace(commander).second) {
          profile.ownedShips.clear();
          profile.activeShipId.clear();
          profile.nextShipSequence = 1;
        }
        helion::ships::OwnedShip owned;
        owned.instanceId = decode(second);
        owned.hullId = third;
        owned.customName = decode(fields[0]);
        if (!instanceIds.emplace(owned.instanceId).second) throw std::runtime_error("duplicate owned ship instance");
        if (!helion::ships::find(owned.hullId)) throw std::runtime_error("unknown owned ship hull");
        owned.flight.x = parseDouble(fields[1]); owned.flight.y = parseDouble(fields[2]);
        owned.flight.vx = parseDouble(fields[3]); owned.flight.vy = parseDouble(fields[4]);
        owned.flight.yaw = parseDouble(fields[5]);
        const auto parseBool = [&parseNumber](const std::string& text) {
          const int parsed = parseNumber(text);
          if (parsed != 0 && parsed != 1) throw std::runtime_error("malformed ship boolean");
          return parsed != 0;
        };
        owned.flight.docked = parseBool(fields[6]);
        owned.flight.cargo = parseNumber(fields[7]); owned.flight.food = parseNumber(fields[8]);
        owned.flight.parts = parseNumber(fields[9]); owned.flight.station = parseNumber(fields[10]);
        owned.flight.hull = parseNumber(fields[11]);
        const int savedMaximumHull = parseNumber(fields[12]);
        owned.flight.fuel = parseDouble(fields[13]);
        const double savedMaximumFuel = parseDouble(fields[14]);
        owned.flight.destroyed = parseBool(fields[15]);
        owned.flight.weaponCooldown = parseDouble(fields[16]);
        owned.hullLevel = parseNumber(fields[17]); owned.engineLevel = parseNumber(fields[18]);
        owned.ownedModules.clear();
        std::size_t start = 0;
        while (start <= fields[19].size()) {
          const auto end = fields[19].find(',', start);
          const std::string id = fields[19].substr(start, end == std::string::npos ? std::string::npos : end - start);
          if (id.empty() || !helion::loadout::find(id) ||
              std::find(owned.ownedModules.begin(), owned.ownedModules.end(), id) != owned.ownedModules.end())
            throw std::runtime_error("malformed owned ship modules");
          owned.ownedModules.push_back(id);
          if (end == std::string::npos) break;
          start = end + 1;
        }
        for (std::size_t slot = 0; slot < helion::loadout::kSlotCount; ++slot)
          owned.fittedModules[slot] = fields[20 + slot];
        owned.visualVariantId = fields.size() == 29 ? fields[28] :
          std::string(helion::ships::defaultVisualVariant(*helion::ships::find(owned.hullId)));
        owned.livery = decode(fields[24]); owned.wearState = fields[25];
        owned.stored = parseBool(fields[26]);
        const bool active = parseBool(fields[27]);
        const auto* defenseId = owned.fittedModules[helion::loadout::slotIndex(helion::loadout::Slot::defense)].empty() ? nullptr :
          helion::loadout::find(owned.fittedModules[helion::loadout::slotIndex(helion::loadout::Slot::defense)]);
        if (owned.flight.hull < 0 || owned.flight.hull > savedMaximumHull ||
            owned.flight.fuel < 0 || owned.flight.fuel > savedMaximumFuel)
          throw std::runtime_error("malformed owned ship state");
        helion::ships::refreshDerivedState(owned, defenseId ? defenseId->hullBonus : 0);
        if (owned.flight.maxHull != savedMaximumHull || std::abs(owned.flight.maxFuel - savedMaximumFuel) > 0.001 ||
            !helion::ships::validate(owned) || active == owned.stored)
          throw std::runtime_error("malformed owned ship state");
        owned.flight.inputAge = 1;
        if (active) {
          if (!profile.activeShipId.empty()) throw std::runtime_error("multiple active ships");
          profile.activeShipId = owned.instanceId;
        }
        profile.ownedShips.push_back(std::move(owned));
        profile.nextShipSequence = static_cast<unsigned>(profile.ownedShips.size() + 1);
      } else if (kind == "D" && !first.empty() && !second.empty() && !third.empty()) {
        std::string extra;
        std::getline(input, extra, '\t');
        const std::string commander = decode(first);
        const auto profile = profiles.find(commander);
        const auto level = helion::galaxy::parseKnowledgeLevel(third);
        if (std::count(line.begin(), line.end(), '\t') != 3 ||
            !extra.empty() || profile == profiles.end() || !level ||
            !knowledgeRecords[commander].emplace(second).second)
          throw std::runtime_error("malformed or duplicate commander galaxy knowledge");
        auto* record = helion::galaxy::findKnowledge(profile->second.galaxyKnowledge, second);
        if (!record) throw std::runtime_error("unknown commander galaxy knowledge system");
        record->level = *level;
      } else if (kind == "T" && !first.empty() && !second.empty() && !third.empty()) {
        std::string expiresText, scopeText, trailing;
        std::getline(input, expiresText, '\t');
        std::getline(input, scopeText, '\t');
        if (std::count(line.begin(), line.end(), '\t') != 5 ||
            expiresText.empty() || scopeText.empty() || std::getline(input, trailing))
          throw std::runtime_error("malformed companion token record");
        std::size_t used = 0;
        const auto expiresAt = std::stoll(expiresText, &used);
        if (used != expiresText.size() || expiresAt <= 0)
          throw std::runtime_error("malformed companion token expiry");
        CompanionTokenRecord token{
          decode(first),
          decode(second),
          decode(third),
          expiresAt,
          decode(scopeText)
        };
        if (!helion::security::isEncodedCompanionTokenHash(token.tokenHash) ||
            token.id.size() != helion::security::kCompanionTokenIdBytes * 2 ||
            token.id.find_first_not_of("0123456789abcdef") != std::string::npos ||
            token.scope != kCompanionProfileReadScope ||
            !companionTokens.emplace(token.id, std::move(token)).second) {
          throw std::runtime_error("malformed companion token record");
        }
      } else if (kind == "M" && !first.empty() && !second.empty()) {
        messages.push_back({decode(first), decode(second)});
      } else if (kind == "G" && !first.empty() && !second.empty() && third.empty()) {
        const std::string id = decode(first);
        const std::string headline = decode(second);
        if (!addGalNetEvent(id, headline)) throw std::runtime_error("duplicate GalNet event");
      } else if (!line.empty()) {
        throw std::runtime_error("malformed persistence record");
      }
    }
    if (ferror(file)) throw std::runtime_error("cannot read persistence file");
    if (knowledgeVersionRecord && galaxyEndRecord != "Z\tGALAXY_END\t" +
        std::to_string(helion::galaxy::kSystemCount) + '\t' + std::to_string(profiles.size()) + '\t' +
        std::to_string(persistenceRecords - 1))
      throw std::runtime_error("missing or invalid persistence terminator");
    const auto matchesLegacyGalaxyPrefix = [&](std::size_t legacyCount) {
      if (galaxyRecords.size() != legacyCount) return false;
      const auto& definitions = helion::galaxy::systems();
      for (std::size_t index = 0; index < legacyCount; ++index)
        if (galaxyRecords.count(std::string(definitions[index].id)) == 0) return false;
      return true;
    };
    const bool legacyGalaxyRecords = matchesLegacyGalaxyPrefix(28) || matchesLegacyGalaxyPrefix(90);
    if (!galaxyRecords.empty() && galaxyRecords.size() != helion::galaxy::kSystemCount &&
        !legacyGalaxyRecords)
      throw std::runtime_error("incomplete galaxy persistence");
    if (!helion::galaxy::validateGalaxy(galaxyState))
      throw std::runtime_error("invalid persisted galaxy state");
    if (receiptCount && *receiptCount != adminReceipts.size()) throw std::runtime_error("incomplete admin ledger");
    galaxyMigrationPending = galaxyRecords.size() != helion::galaxy::kSystemCount;
    if (!marketRecords.empty() && marketRecords.size() != helion::economy::kMarketCount)
      throw std::runtime_error("incomplete market persistence");
    if (!helion::economy::validateMarkets(regionalMarkets, pricingPolicy()))
      throw std::runtime_error("invalid persisted market state");
    economyMigrationPending = marketRecords.empty() || !telemetryRecord;
    if ((knowledgeVersionRecord || !knowledgeRecords.empty()) &&
        galaxyRecords.size() != helion::galaxy::kSystemCount)
      throw std::runtime_error("incomplete current-format galaxy persistence");
    const bool legacyKnowledgeRecords = !knowledgeVersionRecord && knowledgeRecords.empty();
    knowledgeMigrationPending = !knowledgeVersionRecord;
    instanceIds.clear();
    for (auto& [name, profile] : profiles) {
      const auto knowledge = knowledgeRecords.find(name);
      // Seed only after every physical record has loaded; legacy row order must
      // not change the commander's public knowledge baseline.
      if (legacyKnowledgeRecords) profile.galaxyKnowledge = helion::galaxy::seedKnowledge(galaxyState);
      if (!legacyKnowledgeRecords &&
          (knowledge == knowledgeRecords.end() ||
           knowledge->second.size() != helion::galaxy::kSystemCount ||
           !helion::galaxy::validateKnowledge(profile.galaxyKnowledge)))
        throw std::runtime_error("incomplete commander galaxy knowledge");
      if (profilesWithShipRecords.count(name) == 0) {
        if (profile.legacyProjectionOverFuel) throw std::runtime_error("malformed persisted fuel state");
        ownershipMigrationPending = true;
      }
      profile.legacyProjectionOverFuel = false;
      if (profile.ownedShips.empty() || profile.activeShipId.empty()) throw std::runtime_error("profile has no active ship");
      std::size_t activeCount = 0;
      for (const auto& owned : profile.ownedShips) {
        if (!helion::ships::validate(owned)) throw std::runtime_error("invalid persisted owned ship");
        if (!instanceIds.emplace(owned.instanceId).second) throw std::runtime_error("duplicate owned ship instance");
        if (owned.instanceId == profile.activeShipId) ++activeCount;
      }
      if (activeCount != 1) throw std::runtime_error("profile active ship is ambiguous");
      refreshDerivedShipState(profile);
      resetCombatTarget(profile);
    }
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    for (auto it = companionTokens.begin(); it != companionTokens.end();) {
      if (profiles.count(it->second.user) == 0)
        throw std::runtime_error("companion token references missing profile");
      if (it->second.expiresAt <= now) it = companionTokens.erase(it);
      else ++it;
    }
  } catch (...) {
    free(buffer);
    fclose(file);
    throw;
  }
  free(buffer);
  fclose(file);
}

void migrateLegacyProfiles() {
  auto migrated = profiles;
  bool changed = ownershipMigrationPending || environmentMigrationPending || economyMigrationPending ||
                 galaxyMigrationPending || knowledgeMigrationPending;
  for (auto& [name, profile] : migrated) {
    (void)name;
    if (helion::security::migrateLegacyCredential(profile.passwordHash, profile.legacyCredential)) {
      profile.legacyCredential = false;
      changed = true;
    }
  }
  if (!changed) return;
  profiles.swap(migrated);
  try {
    saveStateLocked();
    ownershipMigrationPending = false;
    environmentMigrationPending = false;
    economyMigrationPending = false;
    galaxyMigrationPending = false;
    knowledgeMigrationPending = false;
  }
  catch (...) { profiles.swap(migrated); throw; }
  for (auto& [name, profile] : migrated) {
    (void)name;
    std::fill(profile.passwordHash.begin(), profile.passwordHash.end(), '\0');
  }
}

thread_local std::uint32_t responseRequestId = 0;

class ResponseRequestScope {
 public:
  explicit ResponseRequestScope(std::uint32_t requestId) : previous_(responseRequestId) {
    responseRequestId = requestId;
  }
  ~ResponseRequestScope() { responseRequestId = previous_; }
 private:
  std::uint32_t previous_;
};

bool sendLine(int fd, const std::string& line) {
  // An uncertain committed rename must not produce a success acknowledgement.
  // The lifecycle will interrupt this connection and exit unsuccessfully.
  if (serviceMetrics.storageFatal) return false;
  if (!helion::protocol::validWireLine(line)) return false;
  // Static classifications only; raw responses and credentials never enter logs.
  if (line == "ERR invalid-login" || line == "ERR invalid-companion-token" || line == "ERR companion-token-expired") {
    ++serviceMetrics.authFailures;
    helion::server::operationLog(environmentPolicy, "authentication", "denied");
  }
  if (line.rfind("OK LOGIN", 0) == 0 || line.rfind("OK CREATED", 0) == 0 || line.rfind("OK COMPANION AUTH", 0) == 0)
    helion::server::operationLog(environmentPolicy, "authentication", "success");
  if (line == "ERR scope-denied" || line == "ERR test-environment-required")
    helion::server::operationLog(environmentPolicy, "authorization", "denied");
  std::shared_ptr<Transport> transport;
  {
    std::lock_guard<std::mutex> lock(transportMutex);
    const auto found = transports.find(fd);
    if (found == transports.end()) return false;
    transport = found->second;
  }
  std::lock_guard<std::mutex> lock(transport->writeMutex);
  if (transport->binaryMode) {
    const auto frame = helion::protocol::binary::encodeFrame(
      helion::protocol::binary::MessageType::textResponse, responseRequestId, line);
    return frame && transport->connection->sendAll(*frame);
  }
  return transport->connection->sendAll(line + "\n");
}

bool enableBinaryTransport(int fd) {
  std::shared_ptr<Transport> transport;
  {
    std::lock_guard<std::mutex> lock(transportMutex);
    const auto found = transports.find(fd);
    if (found == transports.end()) return false;
    transport = found->second;
  }
  std::lock_guard<std::mutex> lock(transport->writeMutex);
  if (transport->binaryMode || !transport->connection->sendAll("OK PROTOCOL BINARY 1\n")) return false;
  transport->binaryMode = true;
  return true;
}

struct OutboundResponse {
  enum class Kind { text, flightSnapshot } kind = Kind::text;
  std::string line;
  helion::flight::State flight;
  int credits = 0;
  int experience = 0;

  OutboundResponse(const char* value) : line(value) {}
  OutboundResponse(std::string value) : line(std::move(value)) {}
  static OutboundResponse snapshot(const helion::flight::State& state, int creditsValue,
                                   int experienceValue) {
    OutboundResponse response("");
    response.kind = Kind::flightSnapshot;
    response.flight = state;
    response.credits = creditsValue;
    response.experience = experienceValue;
    return response;
  }
};

bool sendResponse(int fd, const OutboundResponse& response) {
  if (serviceMetrics.storageFatal) return false;
  std::shared_ptr<Transport> transport;
  {
    std::lock_guard<std::mutex> lock(transportMutex);
    const auto found = transports.find(fd);
    if (found == transports.end()) return false;
    transport = found->second;
  }
  std::lock_guard<std::mutex> lock(transport->writeMutex);
  if (response.kind == OutboundResponse::Kind::flightSnapshot) {
    if (!transport->binaryMode)
      return transport->connection->sendAll(
        helion::flight::snapshot(response.flight, response.credits, response.experience) + "\n");
    const auto payload = helion::protocol::binary::encodeFlightSnapshotPayload(
      response.flight, response.credits, response.experience);
    if (!payload) return false;
    const auto frame = helion::protocol::binary::encodeFrame(
      helion::protocol::binary::MessageType::flightSnapshot, responseRequestId, *payload);
    return frame && transport->connection->sendAll(*frame);
  }
  if (!helion::protocol::validWireLine(response.line)) return false;
  if (transport->binaryMode) {
    if (response.line.rfind("FUEL ", 0) == 0 || response.line.rfind("COMBAT STATUS ", 0) == 0)
      return true; // Included in the binary flight snapshot.
    const auto frame = helion::protocol::binary::encodeFrame(
      helion::protocol::binary::MessageType::textResponse, responseRequestId, response.line);
    return frame && transport->connection->sendAll(*frame);
  }
  return transport->connection->sendAll(response.line + "\n");
}

std::string stateLine() {
  std::lock_guard<std::mutex> lock(stateMutex);
  std::ostringstream out;
  out << "STATE profiles=" << profiles.size() << " messages=" << messages.size();
  return out.str();
}

void appendFlightState(std::vector<OutboundResponse>& responses, Profile& profile) {
  const auto& ship = activeShip(profile);
  const auto& hull = helion::ships::definition(ship);
  responses.push_back(OutboundResponse::snapshot(ship.flight, profile.credits, profile.experience));
  responses.push_back(helion::flight::fuelLine(ship.flight));
  responses.push_back("ACTIVE SHIP instance=" + ship.instanceId + " hull=" + ship.hullId +
    " display=" + std::string(hull.model) + " manufacturer=" + std::string(hull.manufacturerId) +
    " operator=" + std::string(hull.operatorId) + " cargo-capacity=" + std::to_string(hull.cargoCapacity) +
    " speed=" + std::to_string(static_cast<int>(hull.topSpeed)) +
    " acceleration=" + std::to_string(static_cast<int>(hull.acceleration)) +
    " handling=" + std::to_string(hull.maneuverability) + " hull-capacity=" +
    std::to_string(ship.flight.maxHull) + " pad=" + helion::ships::padSizeName(hull.padSize) +
    " mass=" + std::to_string(static_cast<int>(hull.mass)) + " livery=" + ship.livery + " wear=" + ship.wearState);
  if (ship.flight.destroyed || !profile.combatEvents.empty())
    responses.push_back(helion::flight::combatStatusLine(ship.flight));
  while (!profile.combatEvents.empty()) {
    responses.push_back(std::move(profile.combatEvents.front()));
    profile.combatEvents.pop_front();
  }
}

void broadcast(const std::string& line) {
  // Broadcasts are unsolicited on every recipient, not replies to the sender's sequence.
  ResponseRequestScope unsolicited(0);
  std::vector<int> recipients;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    recipients.assign(authenticatedClients.begin(), authenticatedClients.end());
  }
  for (const int fd : recipients) sendLine(fd, line);
}

std::vector<helion::flight::Contact> contactsFor(const std::string& user) {
  std::vector<helion::flight::Contact> result;
  const auto current = profiles.find(user);
  if (current != profiles.end() && !current->second.hostile.destroyed && !activeShip(current->second).flight.destroyed) {
    const auto& hostile = current->second.hostile;
    const auto* affiliation = helion::organizations::find(hostile.faction);
    result.push_back({hostile.id, "hostile", hostile.x, hostile.y, 0, false, true, hostile.hull, hostile.maxHull,
                      hostile.faction, affiliation ? std::string(affiliation->wireName) : "Unknown"});
  }
  for (const auto& [name, profile] : profiles) {
    if (name == user) continue;
    const auto& flight = activeShip(profile).flight;
    result.push_back({name, "pilot", flight.x, flight.y, flight.yaw, flight.docked, false, 0, 0, "", ""});
  }
  const double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  result.push_back({"HAULER-7", "hauler", 420.0 + std::cos(t * 0.08) * 180.0,
                    160.0 + std::sin(t * 0.08) * 120.0, 0, false, false, 0, 0, "", ""});
  return result;
}

void applyNpcDamage(Profile& profile) {
  const auto& hostile = profile.hostile;
  auto& flight = activeShip(profile).flight;
  const int before = flight.hull;
  flight.hull = std::max(0, flight.hull - helion::combat::kNpcDamage);
  profile.combatEvents.push_back("COMBAT DAMAGE source=" + hostile.id +
    " damage=" + std::to_string(before - flight.hull) +
    " hull=" + std::to_string(flight.hull));
  if (flight.hull != 0) return;
  flight.destroyed = true;
  flight.docked = false;
  flight.vx = 0;
  flight.vy = 0;
  flight.input = {};
  flight.inputAge = 1;
  flight.cargo = 0;
  profile.combatEvents.push_back("COMBAT DESTROYED player=1 cargo-lost=1 recovery=RECOVER");
}

void resetCombatAfterLaunch(Profile& profile) {
  if (!profile.combatTargetDefeated) return;
  if (profile.combatTargetGeneration == std::numeric_limits<int>::max()) return;
  ++profile.combatTargetGeneration;
  profile.combatTargetDefeated = false;
  resetCombatTarget(profile);
}

void removeClient(int fd) {
  std::lock_guard<std::mutex> lock(stateMutex);
  authenticatedClients.erase(fd);
  for (auto it = clients.begin(); it != clients.end(); ++it) {
    if (*it == fd) {
      clients.erase(it);
      break;
    }
  }
}

// All callers hold stateMutex (or run before listening). Client-supplied text is
// parsed into a narrow operation; no field offset, file path, SQL or script API.
std::string executeTestAdmin(std::string_view payload, const std::string& user, AuthKind kind, int fd, bool withinBudget) {
  using namespace helion::server;
  const auto op = parseAdminOperation(payload);
  AuditRecord audit;
  audit.account = user.empty() ? "anonymous" : user;
  audit.session = std::to_string(getpid()) + ":" + std::to_string(fd);
  audit.action = op ? op->verbText : "INVALID";
  audit.target = op && (profiles.count(op->target) || op->target == "universe") ? op->target : "unknown";
  audit.parameters = op ? op->field : "redacted";
  audit.request = op ? op->id : "invalid";
  auto record = [&](const std::string& result, const std::string& reason) {
    audit.result = result; audit.reason = reason;
    appendAudit(dataPath, environmentPolicy, audit);
  };
  try {
    const auto denied = withinBudget ? authorizeTestAdmin({environmentPolicy, user, kind == AuthKind::player}) :
      std::string("admin-request-limit");
    if (!denied.empty() || !op) {
      const auto reason = denied.empty() ? "invalid-test-operation" : denied;
      record("denied", reason);
      return "ERR " + reason;
    }
    const auto key = digest(user + ":" + op->id);
    const auto fingerprint = digest(op->canonical());
    const auto previous = adminReceipts.find(key);
    if (previous != adminReceipts.end()) {
      if (previous->second.fingerprint != fingerprint) {
        record("denied", "request-id-conflict"); return "ERR request-id-conflict";
      }
      record("replayed", "already-committed");
      return previous->second.result + " replay=1";
    }
    if (adminReceipts.size() >= 4096) throw std::runtime_error("admin-ledger-full");
    const auto found = profiles.find(op->target);
    const bool marketReset = op->verb == AdminVerb::reset && op->target == "universe" &&
      op->field == "markets" && op->value == "seed";
    if (found == profiles.end() && !marketReset) throw std::runtime_error("unknown-target");
    auto next = found == profiles.end() ? Profile{} : found->second;
    auto nextMarkets = regionalMarkets;
    auto nextTelemetry = economicTelemetry;
    auto nextTokens = companionTokens;
    std::string before, after;
    bool numeric = false;
    if (marketReset) {
      const auto summary = [](const helion::economy::MarketState& markets) {
        std::ostringstream out;
        for (const auto& order : markets) out << order.station << ':' << order.commodityId << ':' <<
          order.stock << ':' << order.demand << ':' << order.buyBudget << ';';
        return out.str();
      };
      before = summary(regionalMarkets);
      nextMarkets = helion::economy::seedMarkets(pricingPolicy());
      after = summary(nextMarkets);
      audit.requested = "seed";
    } else if (op->field == "player" && op->verb == AdminVerb::reset && op->value == "starter") {
      before = "gsc:" + std::to_string(next.credits) + ",xp:" + std::to_string(next.experience) +
        ",ships:" + std::to_string(next.ownedShips.size());
      const auto password = next.passwordHash, display = next.display;
      next = Profile{}; next.passwordHash = password; next.display = display;
      initializeReputation(next); next.galaxyKnowledge = helion::galaxy::seedKnowledge(galaxyState);
      addStarterShip(next, op->target);
      resetCombatTarget(next);
      after = "gsc:1500,xp:0,ships:1";
      audit.requested = "starter";
      for (auto it = nextTokens.begin(); it != nextTokens.end();)
        if (it->second.user == op->target) it = nextTokens.erase(it); else ++it;
    } else {
      auto& ship = activeShip(next);
      auto& flight = ship.flight;
      if (op->field == "ship" && op->verb == AdminVerb::grant) {
        const auto* hull = helion::ships::find(op->value);
        if (!hull) throw std::runtime_error("unknown-hull");
        audit.requested = hull->id;
        audit.before = std::to_string(next.ownedShips.size());
        if (!flight.docked || flight.destroyed || !helion::ships::supportsPad(
              helion::flight::kStations[static_cast<std::size_t>(flight.station)].maximumPad, hull->padSize))
          throw std::runtime_error("ship-location-constraint");
        if (next.ownedShips.size() >= helion::ships::registry().size()) throw std::runtime_error("ship-limit");
        if (std::any_of(next.ownedShips.begin(), next.ownedShips.end(),
            [hull](const auto& owned) { return owned.hullId == hull->id; })) throw std::runtime_error("ship-already-owned");
        before = std::to_string(next.ownedShips.size());
        const auto instance = helion::ships::deterministicInstanceId(op->target, next.nextShipSequence++);
        next.ownedShips.push_back(helion::ships::makePurchasedShip(instance, *hull, flight.station));
        after = std::to_string(next.ownedShips.size()); audit.requested = hull->id;
      } else if (op->field == "module" && (op->verb == AdminVerb::grant || op->verb == AdminVerb::revoke)) {
        const auto* module = helion::loadout::find(op->value);
        if (!module) throw std::runtime_error("unknown-module");
        audit.requested = module->id;
        audit.before = std::to_string(ship.ownedModules.size());
        if (!flight.docked || flight.destroyed) throw std::runtime_error("dock-required");
        auto& owned = ship.ownedModules;
        const auto match = std::find(owned.begin(), owned.end(), op->value);
        before = std::to_string(owned.size());
        if (op->verb == AdminVerb::grant) {
          if (match != owned.end()) throw std::runtime_error("module-already-owned");
          owned.push_back(op->value);
        } else {
          if (match == owned.end()) throw std::runtime_error("module-not-owned");
          if (std::find(ship.fittedModules.begin(), ship.fittedModules.end(), op->value) != ship.fittedModules.end())
            throw std::runtime_error("module-is-fitted");
          owned.erase(match);
        }
        after = std::to_string(owned.size()); audit.requested = module->id;
      } else if (op->field == "station" && op->verb == AdminVerb::teleport) {
        int station = -1;
        const auto parsed = std::from_chars(op->value.data(), op->value.data() + op->value.size(), station);
        if (parsed.ec != std::errc{} || parsed.ptr != op->value.data() + op->value.size() ||
            station < 0 || station >= static_cast<int>(helion::flight::kStations.size()) ||
            !helion::ships::supportsPad(helion::flight::kStations[static_cast<std::size_t>(station)].maximumPad,
              helion::ships::definition(ship).padSize)) throw std::runtime_error("invalid-station");
        if (flight.destroyed) throw std::runtime_error("recovery-required");
        before = std::to_string(flight.station);
        flight.station = station; flight.docked = true; flight.destroyed = false;
        flight.x = helion::flight::kStations[static_cast<std::size_t>(station)].x;
        flight.y = helion::flight::kStations[static_cast<std::size_t>(station)].y;
        flight.vx = flight.vy = 0; flight.input = {}; flight.inputAge = 1;
        after = std::to_string(station); audit.requested = after;
        resetCombatTarget(next);
      } else {
        int* destination = nullptr;
        if (op->field == "gsc") destination = &next.credits;
        if (op->field == "experience") destination = &next.experience;
        if (op->field == "salvage") destination = &next.salvage;
        if (op->field == "ore") destination = &flight.cargo;
        if (op->field == "food") destination = &flight.food;
        if (op->field == "parts") destination = &flight.parts;
        if (op->field == "hull") destination = &flight.hull;
        if (!destination && op->field != "fuel") throw std::runtime_error("unsupported-operation");
        if (op->field != "gsc" && op->field != "experience" && op->field != "salvage" &&
            (!flight.docked || flight.destroyed)) throw std::runtime_error("dock-required");
        const long long old = destination ? *destination : static_cast<long long>(flight.fuel);
        before = destination ? std::to_string(*destination) : std::to_string(flight.fuel);
        audit.before = before;
        long long value = old;
        if (op->verb == AdminVerb::get) {
          if (op->value != "-") throw std::runtime_error("get-requires-placeholder");
          audit.requested = "-";
        } else {
          long long amount = 0;
          const auto parsed = std::from_chars(op->value.data(), op->value.data() + op->value.size(), amount);
          if (parsed.ec != std::errc{} || parsed.ptr != op->value.data() + op->value.size() || amount < 0 || amount > 1000000000)
            throw std::runtime_error("invalid-quantity");
          audit.requested = std::to_string(amount);
          if (op->verb == AdminVerb::set) value = amount;
          else if (op->verb == AdminVerb::add) value += amount;
          else if (op->verb == AdminVerb::remove) value -= amount;
          else throw std::runtime_error("invalid-numeric-operation");
          if (value < 0 || value > 1000000000 || (op->field == "hull" && (value == 0 || value > flight.maxHull)) ||
              (op->field == "fuel" && value > flight.maxFuel)) throw std::runtime_error("quantity-out-of-range");
          if (destination) *destination = static_cast<int>(value); else flight.fuel = static_cast<double>(value);
          audit.requested = std::to_string(amount);
        }
        after = op->verb == AdminVerb::get ? before : std::to_string(value);
        numeric = true;
      }
      for (const auto& owned : next.ownedShips)
        if (!helion::ships::validate(owned)) throw std::runtime_error("ship-invariant-violation");
    }
    audit.before = before; audit.after = after;
    if (found != profiles.end()) {
      const long long delta = static_cast<long long>(next.credits) - found->second.credits;
      if (delta != 0) helion::economy::recordCurrency(nextTelemetry, delta > 0 ?
        helion::economy::CurrencyFlow::faucet : helion::economy::CurrencyFlow::sink, std::abs(delta));
    }
    if (!helion::economy::validateMarkets(nextMarkets, pricingPolicy()) ||
        !helion::economy::validateTelemetry(nextTelemetry)) throw std::runtime_error("economy-invariant-violation");
    const std::string response = "OK TESTADMIN id=" + op->id + " field=" + op->field + " value=" + (marketReset ? "seeded" : after);
    if (numeric && op->verb == AdminVerb::get) { record("success", "read-only"); return response; }
    record("intent", "validated"); // Must be durable before any live mutation.
    const auto oldProfile = found == profiles.end() ? Profile{} : found->second;
    const auto oldMarkets = regionalMarkets;
    const auto oldTelemetry = economicTelemetry;
    const auto oldTokens = companionTokens;
    const bool oldDirty = flightStateDirty;
    if (found != profiles.end()) found->second = std::move(next);
    regionalMarkets = nextMarkets; economicTelemetry = nextTelemetry; companionTokens = std::move(nextTokens);
    adminReceipts.emplace(key, AdminReceipt{fingerprint, response});
    try { persistStateLocked(); }
    catch (...) {
      if (found != profiles.end()) found->second = oldProfile;
      regionalMarkets = oldMarkets; economicTelemetry = oldTelemetry; companionTokens = oldTokens;
      adminReceipts.erase(key); flightStateDirty = oldDirty;
      throw std::runtime_error("persistence-failed");
    }
    // A crash here leaves a durable intent and receipt, allowing reconciliation
    // and an idempotent retry. Never undo a committed operation on log failure.
    try { record("success", "committed"); }
    catch (...) { return "ERR audit-outcome-unavailable retry-same-id=1"; }
    return response;
  } catch (const std::exception& error) {
    audit.after = audit.before;
    try { record("failure", error.what()); }
    catch (...) { return "ERR audit-unavailable"; }
    return "ERR " + std::string(error.what());
  }
}

int performTestRecovery(const std::string& action, const std::string& id, const std::string& account) {
  using namespace helion::server;
  AuditRecord audit;
  audit.account = environmentPolicy.accounts.count(account) ? account : "unknown";
  audit.session = "local:" + std::to_string(getpid());
  audit.action = action; audit.target = "TEST-universe"; audit.request = id;
  auto log = [&](const std::string& result, const std::string& reason) {
    audit.result = result; audit.reason = reason;
    appendAudit(dataPath, environmentPolicy, audit);
  };
  try {
    std::string password;
    // Password is supplied on stdin, never argv, environment or an audit record.
    if (!std::getline(std::cin, password)) password.clear();
    const auto actor = profiles.find(account);
    const bool authenticated = actor != profiles.end() && environmentPolicy.admits(account) &&
      actor->second.passwordHash == environmentPolicy.invitationCredentials.at(account) &&
      helion::security::verifyPassword(password, actor->second.passwordHash);
    password.assign(password.size(), '\0');
    const auto denied = authorizeTestAdmin({environmentPolicy, account, authenticated});
    if (!denied.empty()) { log("denied", denied); std::cerr << denied << '\n'; return 1; }
    namespace fs = std::filesystem;
    const auto directory = fs::path(dataPath).parent_path() / ".helion-test-snapshots";
    if (!fs::exists(directory)) {
      if (mkdir(directory.c_str(), 0700)) throw std::runtime_error("cannot-create-snapshot-directory");
    }
    struct stat st{};
    if (lstat(directory.c_str(), &st) || !S_ISDIR(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 077) != 0)
      throw std::runtime_error("unsafe-snapshot-directory");
    const auto snapshot = (directory / (id + ".snapshot")).string();
    if (action == "SNAPSHOT") {
      persistStateLocked();
      const auto content = readPrivateFile(dataPath, 64 * 1024 * 1024);
      const auto header = "HELION_TEST_SNAPSHOT_V1 test " + environmentPolicy.instance + " " +
        (environmentPolicy.cheapEconomy() ? "convenience" : "production-like") + " " + id + " " +
        std::to_string(std::time(nullptr)) + " native-0.2.0-admin1 " + digest(content) + "\n";
      audit.before = digest(content); audit.after = audit.before; audit.requested = id;
      log("intent", "snapshot");
      writePrivateFile(snapshot, header + content);
      log("success", "snapshot-created");
      return 0;
    }
    if (action != "RESTORE" && action != "RESET") throw std::runtime_error("unsupported-recovery");
    const auto key = digest(account + ":recovery:" + id);
    if (adminReceipts.count(key)) throw std::runtime_error("recovery-id-already-used");
    if (adminReceipts.size() >= 4096) throw std::runtime_error("admin-ledger-full");
    const auto existingReceipts = adminReceipts;
    const auto actorProfile = actor->second;
    audit.before = digest(readPrivateFile(dataPath, 64 * 1024 * 1024));
    audit.requested = action + ":" + id;
    if (action == "RESTORE") {
      const auto content = readPrivateFile(snapshot, 64 * 1024 * 1024);
      const auto end = content.find('\n');
      if (end == std::string::npos) throw std::runtime_error("invalid-snapshot-header");
      std::istringstream header(content.substr(0, end));
      std::string version, env, instance, policy, snapshotId, timestamp, build, checksum, extra;
      if (!(header >> version >> env >> instance >> policy >> snapshotId >> timestamp >> build >> checksum) ||
          (header >> extra) || version != "HELION_TEST_SNAPSHOT_V1" || env != "test" ||
          instance != environmentPolicy.instance || snapshotId != id || build != "native-0.2.0-admin1" ||
          timestamp.empty() || timestamp.find_first_not_of("0123456789") != std::string::npos ||
          policy != (environmentPolicy.cheapEconomy() ? "convenience" : "production-like") ||
          checksum != digest(std::string_view(content).substr(end + 1))) throw std::runtime_error("snapshot-compatibility-failed");
      // Validate with the normal strict loader in this offline process. The live
      // file is untouched on any failure, including partial/inconsistent saves.
      const auto original = dataPath;
      std::string candidate = dataPath + ".restore.XXXXXX";
      const int fd = mkstemp(candidate.data());
      if (fd < 0) throw std::runtime_error("cannot-stage-restore");
      close(fd);
      writePrivateFile(candidate, std::string_view(content).substr(end + 1), false);
      try {
        profiles.clear(); companionTokens.clear(); messages.clear(); galnetEvents.clear(); adminReceipts.clear();
        galaxyState = helion::galaxy::seedState(); regionalMarkets = helion::economy::seedMarkets(pricingPolicy());
        economicTelemetry = {};
        dataPath = candidate;
        loadState();
        dataPath = original;
        unlink(candidate.c_str());
      } catch (...) { dataPath = original; unlink(candidate.c_str()); throw; }
      if (!profiles.count(account) || profiles.at(account).passwordHash != actorProfile.passwordHash)
        throw std::runtime_error("snapshot-operator-mismatch");
      for (const auto& [receiptKey, receipt] : existingReceipts) {
        const auto old = adminReceipts.find(receiptKey);
        if (old != adminReceipts.end() && old->second.fingerprint != receipt.fingerprint)
          throw std::runtime_error("snapshot-ledger-conflict");
        adminReceipts[receiptKey] = receipt;
      }
    } else {
      // Keep identities/passwords, reset implemented progression, ships, market,
      // knowledge and messages. The invitation policy and audit never roll back.
      for (auto& [name, profile] : profiles) {
        const auto passwordHash = profile.passwordHash, display = profile.display;
        profile = Profile{}; profile.passwordHash = passwordHash; profile.display = display;
        initializeReputation(profile); addStarterShip(profile, name); resetCombatTarget(profile);
        profile.galaxyKnowledge = helion::galaxy::seedKnowledge(helion::galaxy::seedState());
      }
      regionalMarkets = helion::economy::seedMarkets(pricingPolicy());
      galaxyState = helion::galaxy::seedState(); messages.clear(); galnetEvents.clear(); economicTelemetry = {};
      for (const auto& entry : profiles) {
        (void)entry;
        helion::economy::recordCurrency(economicTelemetry, helion::economy::CurrencyFlow::faucet, 1500);
        economicTelemetry.starterGrants += 1500;
      }
    }
    if (adminReceipts.size() >= 4096) throw std::runtime_error("admin-ledger-full");
    companionTokens.clear(); // Restores never resurrect old bearer credentials.
    adminReceipts.emplace(key, AdminReceipt{digest(action + ":" + id), "OK RECOVERY"});
    log("intent", "validated-offline-recovery");
    persistStateLocked();
    audit.after = digest(readPrivateFile(dataPath, 64 * 1024 * 1024));
    log("success", "recovery-committed-companion-tokens-revoked");
    return 0;
  } catch (const std::exception& error) {
    try { log("failure", "recovery-validation-or-io-failed"); } catch (...) {}
    std::cerr << "TEST recovery failed: " << error.what() << '\n'; return 1;
  }
}

// Strict persistence validation in a disposable offline process. Never migrate
// or replace the authoritative file as part of backup verification/planning.
void validateBackupPayload(std::string_view payload) {
  const auto original = dataPath;
  std::string candidate = dataPath + ".verify.XXXXXX";
  const int fd = mkstemp(candidate.data());
  if (fd < 0) throw std::runtime_error("cannot stage backup verification");
  close(fd);
  try {
    helion::server::writePrivateFile(candidate, payload, false);
    profiles.clear(); companionTokens.clear(); messages.clear(); galnetEvents.clear(); adminReceipts.clear();
    regionalMarkets = helion::economy::seedMarkets(pricingPolicy());
    galaxyState = helion::galaxy::seedState(); economicTelemetry = {};
    dataPath = candidate;
    loadState();
    if (payload.empty() || payload.substr(0, 13) != "E\tproduction\n")
      throw std::runtime_error("Production backup requires tagged persistence");
    dataPath = original;
    unlink(candidate.c_str());
  } catch (...) { dataPath = original; unlink(candidate.c_str()); throw; }
}

int performProductionOperation(const std::string& action, const std::string& id, const std::string& reason) {
  using namespace helion::server;
  // OS ownership/exclusive lock is the operator boundary. No player/admin role
  // grants this access. The independent append-only journal never rolls back.
  auto audit = [&](const char* result) {
    const std::string record = "{\"timestamp\":" + std::to_string(std::time(nullptr)) +
      ",\"environment\":\"production\",\"uid\":" + std::to_string(geteuid()) +
      ",\"action\":\"" + action + "\",\"backup\":\"" + id + "\",\"reason\":\"" + reason +
      "\",\"result\":\"" + result + "\"}\n";
    writePrivateFile(dataPath + ".operations.jsonl", record, false);
    syncDirectory(dataPath + ".operations.jsonl");
  };
  try {
    audit("intent");
    if (action == "BACKUP") {
      // loadState already validated live state. Preserve its exact original
      // format, including migration candidates, rather than migrating first.
      const auto payload = readPrivateFile(dataPath, 64 * 1024 * 1024);
      validateBackupPayload(payload);
      createBackup(dataPath, environmentPolicy, id);
      ++serviceMetrics.backups; serviceMetrics.lastBackup = std::time(nullptr);
    } else {
      const auto payload = readBackup(dataPath, environmentPolicy, id);
      validateBackupPayload(payload);
      // A plan is deliberately non-destructive, even if live state is corrupt.
      // Restoration must be separately reviewed with a preserved current copy.
      std::cout << "{\"backup_id\":\"" << id << "\",\"sha256\":\"" << digest(payload)
        << "\",\"compatible\":true,\"replaces_live_state\":false}\n";
    }
    audit("success");
    publishStatus(dataPath, environmentPolicy, ServicePhase::offline, serviceMetrics, 0, 0, 0, action == "BACKUP");
    operationLog(environmentPolicy, "offline-operation", "verified");
    return 0;
  } catch (...) {
    if (action == "BACKUP") ++serviceMetrics.backupFailures;
    try { publishStatus(dataPath, environmentPolicy, ServicePhase::fatal, serviceMetrics, 0, 0, 0, false); } catch (...) {}
    try { audit("failed"); } catch (...) {}
    operationLog(environmentPolicy, "offline-operation", "failed");
    std::cerr << "Production operation failed; live state was not replaced\n";
    return 1;
  }
}

void clientLoop(int fd, helion::tls::Connection& connection) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    clients.push_back(fd);
  }
  sendLine(fd, helion::protocol::welcomeLine());
  sendLine(fd, "INFO commands=PROTOCOL CREATE LOGIN COMPANION CHAT PROFILE STATE GALNET ECONOMY GALAXY CONTACTS BUY SELL MISSION ACCEPT TURNIN CAREER UPGRADE REPAIR REFUEL OUTFIT SHIPYARD FIRE RECOVER LAUNCH INPUT FLIGHT MINE DOCK QUIT legacy-version=2 binary-version=1");

  std::string user;
  AuthKind authKind = AuthKind::none;
  std::string authenticatedTokenId;
  int authFailures = 0;
  int authRequests = 0;
  int adminRequests = 0;
  helion::protocol::LineDecoder decoder;
  helion::protocol::binary::Decoder binaryDecoder;
  bool binaryMode = false;
  std::uint32_t lastBinaryRequestId = 0;
  bool running = true;
  auto lastCompleteLine = std::chrono::steady_clock::now();
  char bytes[1024];
  while (running) {
    const ssize_t received = connection.receive(bytes, sizeof(bytes));
    if (received == 0) break;
    if (received < 0) {
      if (errno == EINTR) continue;
      break;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - lastCompleteLine > std::chrono::seconds(kReadTimeoutSeconds)) break;
    struct IncomingFrame {
      helion::protocol::Frame frame;
      std::uint32_t requestId = 0;
      std::optional<helion::protocol::Request> parsedRequest;
      std::string terminalError;
    };
    std::vector<IncomingFrame> incoming;
    if (binaryMode) {
      for (const auto& event : binaryDecoder.feed(
             std::string_view(bytes, static_cast<std::size_t>(received)))) {
        if (event.kind != helion::protocol::binary::DecodeKind::frame) {
          incoming.push_back({{helion::protocol::FrameKind::malformed, {}}, 0, std::nullopt,
            std::string("ERR ") + helion::protocol::binary::decodeKindName(event.kind)});
          break;
        }
        if (event.frame.requestId == 0 || event.frame.requestId <= lastBinaryRequestId) {
          incoming.push_back({{helion::protocol::FrameKind::malformed, {}}, event.frame.requestId,
            std::nullopt, "ERR replayed-request"});
          break;
        }
        lastBinaryRequestId = event.frame.requestId;
        if (event.frame.type == helion::protocol::binary::MessageType::input) {
          const auto input = helion::protocol::binary::decodeInputPayload(std::string_view(
            reinterpret_cast<const char*>(event.frame.payload.data()), event.frame.payload.size()));
          if (!input) {
            incoming.push_back({{helion::protocol::FrameKind::malformed, {}}, event.frame.requestId,
              std::nullopt, "ERR malformed-input"});
            break;
          }
          helion::protocol::Request request;
          request.command = helion::protocol::Command::input;
          request.thrust = input->thrust;
          request.turn = input->turn;
          request.brake = input->brake;
          incoming.push_back({{helion::protocol::FrameKind::line, "INPUT"},
                              event.frame.requestId, std::move(request), {}});
          continue;
        }
        if (event.frame.type != helion::protocol::binary::MessageType::textRequest) {
          incoming.push_back({{helion::protocol::FrameKind::malformed, {}}, event.frame.requestId,
            std::nullopt, "ERR invalid-message-direction"});
          break;
        }
        std::string line(event.frame.payload.begin(), event.frame.payload.end());
        if (!helion::protocol::validWireLine(line)) {
          incoming.push_back({{helion::protocol::FrameKind::malformed, {}}, event.frame.requestId,
            std::nullopt, "ERR malformed-message"});
          break;
        }
        incoming.push_back({{helion::protocol::FrameKind::line, std::move(line)},
                            event.frame.requestId, std::nullopt, {}});
      }
    } else {
      for (auto& frame : decoder.feed(std::string_view(bytes, static_cast<std::size_t>(received))))
        incoming.push_back({std::move(frame), 0, std::nullopt, {}});
    }
    for (const auto& incomingFrame : incoming) {
      ++serviceMetrics.requests;
      const auto& frame = incomingFrame.frame;
      ResponseRequestScope responseScope(incomingFrame.requestId);
      if (draining.load() || serviceMetrics.storageFatal) { sendLine(fd, "ERR service-draining"); running = false; break; }
      if (incomingFrame.frame.kind != helion::protocol::FrameKind::line) {
        ++serviceMetrics.malformed;
        helion::server::operationLog(environmentPolicy, "protocol", "rejected");
      }
      // Commit valid prefixes and emit their replies before a terminal error,
      // independent of TLS read segmentation. Never dispatch trailing frames.
      if (!incomingFrame.terminalError.empty()) {
        sendLine(fd, incomingFrame.terminalError);
        running = false;
        break;
      }
      lastCompleteLine = now;
      if (frame.kind != helion::protocol::FrameKind::line) {
        sendLine(fd, frame.kind == helion::protocol::FrameKind::too_long ? "ERR line-too-long" : "ERR malformed-message");
        continue;
      }
      const auto request = incomingFrame.parsedRequest ? *incomingFrame.parsedRequest :
        helion::protocol::parseRequest(frame.line);
      if (request.command == helion::protocol::Command::protocol) {
        if (binaryMode || !user.empty()) { sendLine(fd, "ERR protocol-negotiation-closed"); continue; }
        if (request.protocolVersion != helion::protocol::binary::kVersion) {
          sendLine(fd, "ERR CLIENT_UPDATE_REQUIRED binary-version=" +
            std::to_string(helion::protocol::binary::kVersion));
          continue;
        }
        if (incoming.size() != 1 || !decoder.atMessageBoundary() || !enableBinaryTransport(fd)) {
          sendLine(fd, "ERR protocol-negotiation-failed");
          running = false;
          break;
        }
        binaryMode = true;
        continue;
      }
      const bool authRequest = request.command == helion::protocol::Command::create ||
        request.command == helion::protocol::Command::login ||
        request.command == helion::protocol::Command::companion_auth ||
        frame.line.rfind("CREATE", 0) == 0 || frame.line.rfind("LOGIN", 0) == 0 ||
        frame.line.rfind("COMPANION AUTH", 0) == 0;
      if (authRequest && ++authRequests > kMaxAuthRequests) {
        sendLine(fd, "ERR too-many-auth-attempts");
        running = false;
        break;
      }
      if (request.command == helion::protocol::Command::invalid) {
        ++serviceMetrics.malformed;
        helion::server::operationLog(environmentPolicy, "request", "malformed");
        sendLine(fd, "ERR " + request.error);
        continue;
      }
      if (request.command == helion::protocol::Command::test_admin) {
        const bool withinBudget = ++adminRequests <= 64;
        std::string response;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          response = executeTestAdmin(request.payload, user, authKind, fd, withinBudget);
        }
        sendLine(fd, response);
        if (!withinBudget) { running = false; break; }
        continue;
      }
      if (authKind == AuthKind::companion &&
          request.command != helion::protocol::Command::profile &&
          request.command != helion::protocol::Command::state &&
          request.command != helion::protocol::Command::quit) {
        sendLine(fd, "ERR scope-denied");
        continue;
      }
      if (request.command == helion::protocol::Command::create) {
        const auto& name = request.first;
        const auto& password = request.second;
        const auto& display = request.payload;
        if (!environmentPolicy.admits(name) ||
            (runtimeEnvironment == helion::server::RuntimeEnvironment::private_test &&
             !helion::security::verifyPassword(password, environmentPolicy.invitationCredentials.at(name)))) {
          sendLine(fd, "ERR private-test-access-denied");
          if (++authFailures >= kMaxAuthFailures) { running = false; break; }
          continue;
        }
        if (password.size() < helion::security::kMinNewPassword || password.size() > helion::security::kMaxPassword) {
          sendLine(fd, "ERR password-length");
          if (++authFailures >= kMaxAuthFailures) { running = false; break; }
          continue;
        }
        std::string encoded;
        try { encoded = helion::security::hashPassword(password); }
        catch (const std::exception&) { sendLine(fd, "ERR password-hashing-unavailable"); continue; }
        std::string response;
        bool tooManyFailures = false;
        bool created = false;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          if (profiles.count(name) != 0) {
            response = "ERR profile-exists";
            tooManyFailures = ++authFailures >= kMaxAuthFailures;
          } else {
            Profile profile;
            initializeReputation(profile);
            profile.galaxyKnowledge = helion::galaxy::seedKnowledge(galaxyState);
            profile.passwordHash = runtimeEnvironment == helion::server::RuntimeEnvironment::private_test ?
              environmentPolicy.invitationCredentials.at(name) : encoded;
            profile.display = display;
            addStarterShip(profile, name);
            profiles.emplace(name, std::move(profile));
            const auto telemetryBefore = economicTelemetry;
            helion::economy::recordCurrency(economicTelemetry, helion::economy::CurrencyFlow::faucet, 1500);
            economicTelemetry.starterGrants += 1500;
            try {
              persistStateLocked();
              response = "OK CREATED user=" + name + " display=" + display;
              created = true;
            } catch (const std::exception& error) {
              profiles.erase(name);
              economicTelemetry = telemetryBefore;
              response = "ERR persistence-failed";
              std::cerr << error.what() << '\n';
            }
          }
        }
        if (created) {
          user = name;
          std::lock_guard<std::mutex> lock(stateMutex);
          authenticatedClients.insert(fd);
          authKind = AuthKind::player;
        }
        sendLine(fd, response);
        if (tooManyFailures) { running = false; break; }
      } else if (request.command == helion::protocol::Command::login) {
        const auto& name = request.first;
        const auto& password = request.second;
        std::string encoded;
        std::string display;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          const auto it = profiles.find(name);
          if (it != profiles.end()) { encoded = it->second.passwordHash; display = it->second.display; }
        }
        const bool found = !encoded.empty();
        if (!found) encoded = dummyPasswordHash;
        const bool authenticated = helion::security::verifyPassword(password, encoded);
        if (!found || !authenticated || !environmentPolicy.admits(name) ||
            (runtimeEnvironment == helion::server::RuntimeEnvironment::private_test &&
             encoded != environmentPolicy.invitationCredentials.at(name))) {
          sendLine(fd, "ERR invalid-login");
          if (++authFailures >= kMaxAuthFailures) { sendLine(fd, "ERR too-many-auth-attempts"); running = false; break; }
        } else {
          user = name;
          {
            std::lock_guard<std::mutex> lock(stateMutex);
            authenticatedClients.insert(fd);
          }
          authKind = AuthKind::player;
          sendLine(fd, "OK LOGIN user=" + name + " display=" + display);
        }
      } else if (request.command == helion::protocol::Command::companion_auth) {
        const auto tokenId = helion::security::companionTokenId(request.second);
        bool authenticated = false;
        bool expired = false;
        std::string tokenUser;
        std::string display;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          const auto token = companionTokens.find(tokenId);
          if (token != companionTokens.end()) {
            expired = token->second.expiresAt <= static_cast<std::int64_t>(std::time(nullptr));
            if (!expired && helion::security::verifyCompanionToken(
                    request.second, token->second.id, token->second.tokenHash)) {
              const auto profile = profiles.find(token->second.user);
              if (profile != profiles.end() && environmentPolicy.admits(token->second.user) &&
                  (runtimeEnvironment != helion::server::RuntimeEnvironment::private_test ||
                   profile->second.passwordHash == environmentPolicy.invitationCredentials.at(token->second.user))) {
                authenticated = true;
                tokenUser = token->second.user;
                display = profile->second.display;
                // A player session may opt into companion scope, but must
                // immediately leave the gameplay broadcast recipient set.
                authenticatedClients.erase(fd);
              }
            }
          }
        }
        if (!authenticated) {
          sendLine(fd, expired ? "ERR companion-token-expired" : "ERR invalid-companion-token");
          if (++authFailures >= kMaxAuthFailures) {
            sendLine(fd, "ERR too-many-auth-attempts");
            running = false;
            break;
          }
        } else {
          user = tokenUser;
          authKind = AuthKind::companion;
          authenticatedTokenId = tokenId;
          sendLine(fd, "OK COMPANION AUTH user=" + user + " display=" + display +
            " scope=" + kCompanionProfileReadScope);
        }
      } else if (request.command == helion::protocol::Command::companion_issue) {
        if (user.empty()) {
          sendLine(fd, "ERR login-required");
        } else if (authKind != AuthKind::player) {
          sendLine(fd, "ERR scope-denied");
        } else {
          std::string response;
          try {
            auto material = helion::security::issueCompanionToken();
            const auto expiresAt = static_cast<std::int64_t>(std::time(nullptr)) + kCompanionTokenLifetimeSeconds;
            bool saved = false;
            {
              std::lock_guard<std::mutex> lock(stateMutex);
              std::size_t activeForUser = 0;
              const auto nowEpoch = static_cast<std::int64_t>(std::time(nullptr));
              for (const auto& [id, token] : companionTokens) {
                (void)id;
                if (token.user == user && token.expiresAt > nowEpoch) ++activeForUser;
              }
              if (activeForUser >= kMaxCompanionTokensPerUser) {
                response = "ERR companion-token-limit";
              } else if (companionTokens.count(material.id) != 0) {
                response = "ERR companion-token-generation";
              } else {
                CompanionTokenRecord record{
                  material.id,
                  user,
                  material.hash,
                  expiresAt,
                  kCompanionProfileReadScope
                };
                companionTokens.emplace(material.id, record);
                try {
                  persistStateLocked();
                  saved = true;
                } catch (const std::exception& error) {
                  companionTokens.erase(material.id);
                  std::cerr << error.what() << '\n';
                  response = "ERR persistence-failed";
                }
              }
            }
            if (saved) {
              response = "OK COMPANION ISSUED id=" + material.id + " token=" + material.token +
                " expires=" + std::to_string(expiresAt) + " scope=" + kCompanionProfileReadScope;
            }
            std::fill(material.token.begin(), material.token.end(), '\0');
          } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            response = "ERR companion-token-generation";
          }
          sendLine(fd, response);
        }
      } else if (request.command == helion::protocol::Command::companion_list) {
        if (user.empty()) {
          sendLine(fd, "ERR login-required");
        } else if (authKind != AuthKind::player) {
          sendLine(fd, "ERR scope-denied");
        } else {
          std::vector<std::string> responses;
          {
            std::lock_guard<std::mutex> lock(stateMutex);
            const auto nowEpoch = static_cast<std::int64_t>(std::time(nullptr));
            for (const auto& [id, token] : companionTokens) {
              if (token.user == user && token.expiresAt > nowEpoch) {
                responses.push_back("COMPANION TOKEN id=" + id +
                  " expires=" + std::to_string(token.expiresAt) + " scope=" + token.scope);
              }
            }
          }
          responses.push_back("COMPANION END");
          for (const auto& response : responses) sendLine(fd, response);
        }
      } else if (request.command == helion::protocol::Command::companion_revoke) {
        if (user.empty()) {
          sendLine(fd, "ERR login-required");
        } else if (authKind != AuthKind::player) {
          sendLine(fd, "ERR scope-denied");
        } else {
          std::string response;
          {
            std::lock_guard<std::mutex> lock(stateMutex);
            const auto found = companionTokens.find(request.second);
            if (found == companionTokens.end() || found->second.user != user) {
              response = "ERR companion-token-not-found";
            } else {
              const auto removed = found->second;
              companionTokens.erase(found);
              try {
                persistStateLocked();
                response = "OK COMPANION REVOKED id=" + request.second;
              } catch (const std::exception& error) {
                companionTokens.emplace(removed.id, removed);
                response = "ERR persistence-failed";
                std::cerr << error.what() << '\n';
              }
            }
          }
          sendLine(fd, response);
        }
      } else if (request.command == helion::protocol::Command::chat) {
        const auto& text = request.payload;
        if (user.empty()) sendLine(fd, "ERR login-required");
        else {
          bool saved = false;
          {
            std::lock_guard<std::mutex> lock(stateMutex);
            messages.push_back({user, text});
            try { persistStateLocked(); saved = true; }
            catch (const std::exception& error) {
              messages.pop_back();
              std::cerr << error.what() << '\n';
            }
          }
          if (!saved) { sendLine(fd, "ERR persistence-failed"); continue; }
          broadcast("CHAT user=" + user + " text=" + text);
        }
      } else if (request.command == helion::protocol::Command::profile) {
        if (user.empty()) {
          sendLine(fd, "ERR login-required");
        } else {
          std::string response;
          {
            std::lock_guard<std::mutex> lock(stateMutex);
            const auto it = profiles.find(user);
            const auto token = companionTokens.find(authenticatedTokenId);
            // Check the live grant while holding the same lock as revocation
            // and the profile read, including on already authenticated peers.
            if (authKind == AuthKind::companion &&
                (token == companionTokens.end() || token->second.user != user ||
                 token->second.scope != kCompanionProfileReadScope))
              response = "ERR invalid-companion-token";
            else if (authKind == AuthKind::companion &&
                     token->second.expiresAt <= static_cast<std::int64_t>(std::time(nullptr)))
              response = "ERR companion-token-expired";
            else if (it == profiles.end()) response = "ERR profile-missing";
            else {
              const Profile& profile = it->second;
              const auto& ship = activeShip(profile);
              const auto& flight = ship.flight;
              response = "PROFILE user=" + user + " display=" + profile.display +
                " faction=" + profile.faction + " ship=" + (ship.hullId == "SIDEWINDER" ? "sidewinder" : ship.hullId) +
                " active-ship-id=" + ship.instanceId + " hull-id=" + ship.hullId +
                " owned-ships=" + std::to_string(profile.ownedShips.size()) +
                " credits=" + std::to_string(profile.credits) +
                " experience=" + std::to_string(profile.experience) +
                " hull=" + std::to_string(flight.hull) +
                " max-hull=" + std::to_string(flight.maxHull) +
                " fuel=" + std::to_string(flight.fuel) +
                " max-fuel=" + std::to_string(flight.maxFuel) +
                " engine-level=" + std::to_string(ship.engineLevel) +
                " hull-level=" + std::to_string(ship.hullLevel) +
                " mission-stage=" + std::to_string(profile.missionStage) +
                " mission-ore-mined=" + std::to_string(profile.missionOreMined) +
                " salvage=" + std::to_string(profile.salvage) +
                " destroyed=" + std::to_string(flight.destroyed) +
                " organizations=" + helion::organizations::organizationSummary() +
                " reputation=" + helion::organizations::reputationSummary(profile.reputation) +
                " " + loadoutLine(profile) +
                " environment=" + std::string(helion::server::runtimeEnvironmentName(runtimeEnvironment)) +
                " role=" + std::string(helion::server::roleName(environmentPolicy.role(user))) +
                " channel=" + std::string(environmentPolicy.releaseChannel());
            }
          }
          sendLine(fd, response);
        }
      } else if (request.command == helion::protocol::Command::galnet) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          for (const auto& event : galnetEvents)
            responses.push_back("GALNET id=" + event.id + " headline=" + event.headline);
          responses.push_back("GALNET END");
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::economy) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          const auto& profile = profiles.at(user);
          const auto& ship = activeShip(profile);
          if (!ship.flight.docked) responses.push_back("ERR dock-required");
          else {
            for (const auto& definition : helion::economy::marketDefinitions()) {
              if (definition.station != ship.flight.station) continue;
              const auto* order = helion::economy::findOrder(regionalMarkets, definition.station,
                                                              definition.commodityId);
              if (order) responses.push_back(helion::economy::marketLine(definition, *order, pricingPolicy()));
            }
            responses.push_back(helion::economy::telemetryLine(economicTelemetry, creditsInCirculation()));
            responses.push_back("ECONOMY END policy=" + std::string(
              pricingPolicy() == helion::economy::PricingPolicy::testConvenience ? "TEST_100" : "PRODUCTION_LIKE"));
          }
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::galaxy) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          const auto& ship = activeShip(profiles.at(user));
          const auto station = static_cast<std::size_t>(ship.flight.station);
          const std::string currentSystem = station < helion::flight::kStations.size() ?
            helion::flight::kStations[station].systemId : "UNKNOWN";
          const auto& knowledge = profiles.at(user).galaxyKnowledge;
          std::set<std::string_view> locatedSystems;
          std::size_t knownSystems = 0;
          for (const auto& record : knowledge) {
            knownSystems += record.level != helion::galaxy::KnowledgeLevel::undetected;
            if (record.level == helion::galaxy::KnowledgeLevel::located ||
                record.level == helion::galaxy::KnowledgeLevel::surveyed)
              locatedSystems.emplace(record.systemId);
          }
          const auto physicalBorders = helion::galaxy::deriveBorders(galaxyState);
          std::vector<helion::galaxy::Border> visibleBorders;
          for (const auto& border : physicalBorders)
            if (locatedSystems.count(border.firstSystemId) != 0 &&
                locatedSystems.count(border.secondSystemId) != 0)
              visibleBorders.push_back(border);
          std::size_t visibleGates = 0;
          for (const auto& connection : helion::galaxy::gateConnections())
            visibleGates += locatedSystems.count(connection.firstSystemId) != 0 &&
                            locatedSystems.count(connection.secondSystemId) != 0;
          responses.push_back("GALAXY BEGIN current=" + currentSystem +
            " known-space-systems=" + std::to_string(helion::galaxy::systems().size()) +
            " known-systems=" + std::to_string(knownSystems) +
            " located-systems=" + std::to_string(locatedSystems.size()) +
            " visible-gates=" + std::to_string(visibleGates) +
            " systems=" + std::to_string(knownSystems) + " gates=" + std::to_string(visibleGates));
          for (const auto& territory : helion::galaxy::factionTerritories())
            if (locatedSystems.count(territory.capitalSystemId) != 0 &&
                locatedSystems.count(territory.commerceHubSystemId) != 0)
              responses.push_back(helion::galaxy::territoryLine(territory));
          for (const auto& definition : helion::galaxy::systems()) {
            const auto* state = helion::galaxy::findState(galaxyState, definition.id);
            const auto* record = helion::galaxy::findKnowledge(knowledge, definition.id);
            if (!state || !record) continue;
            const auto line = helion::galaxy::authorizedSystemLine(definition, *state, *record);
            if (line) responses.push_back(*line);
          }
          for (const auto& connection : helion::galaxy::gateConnections())
            if (locatedSystems.count(connection.firstSystemId) != 0 &&
                locatedSystems.count(connection.secondSystemId) != 0)
              responses.push_back(helion::galaxy::gateLine(connection));
          for (const auto& border : visibleBorders)
            responses.push_back(helion::galaxy::borderLine(border));
          responses.push_back("GALAXY END known-space-systems=" +
            std::to_string(helion::galaxy::systems().size()) +
            " known-systems=" + std::to_string(knownSystems) +
            " located-systems=" + std::to_string(locatedSystems.size()) +
            " visible-gates=" + std::to_string(visibleGates) +
            " visible-borders=" + std::to_string(visibleBorders.size()) +
            " systems=" + std::to_string(knownSystems) + " gates=" + std::to_string(visibleGates) +
            " borders=" + std::to_string(visibleBorders.size()));
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::career) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          if (!request.first.empty()) {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            const auto galnetBefore = galnetEvents;
            const auto telemetryBefore = economicTelemetry;
            const auto result = helion::career::act(profile.career, profile.missionStage, activeShip(profile).flight,
              profile.credits, profile.experience, fittedModule(profile, helion::loadout::Slot::weapon) != nullptr,
              request.first, request.second);
            if (result.line.rfind("OK", 0) == 0) {
              try {
                const int payout = profile.credits - before.credits;
                if (payout > 0) {
                  helion::economy::recordCurrency(economicTelemetry, helion::economy::CurrencyFlow::faucet, payout);
                  economicTelemetry.missionPayouts += payout;
                }
                if (result.keplerDelta) changeStanding(profile, "authority.kepler", result.keplerDelta);
                if (request.first == "ACCEPT" && request.second == helion::career::kSupply)
                  addGalNetEvent("kepler-supply-request", "Kepler Authority requests two industrial parts from Cinder after Red Wake disruption.");
                else if (request.first == "TURNIN" && request.second == helion::career::kSupply)
                {
                  addGalNetEvent("kepler-supply-completed", "Your Cinder supplies reached Kepler; local repair crews resume work.");
                  addGalNetEvent("red-wake-response-available", "Kepler Authority authorizes a Red Wake response; fit a pulse laser before accepting.");
                }
                else if (request.first == "ACCEPT" && request.second == helion::career::kResponse)
                  addGalNetEvent("red-wake-response-authorized", "Kepler Authority authorizes a Red Wake response in the local sector.");
                persistStateLocked();
                responses.push_back(result.line);
                if (result.keplerDelta) responses.push_back(reputationLine(profile, "authority.kepler", result.keplerDelta));
              } catch (const std::exception& error) {
                profile = before; flightStateDirty = dirtyBefore; galnetEvents = galnetBefore;
                economicTelemetry = telemetryBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            } else responses.push_back(result.line);
          }
          responses.push_back(helion::career::status(profile.career, profile.missionStage));
          appendFlightState(responses, profile);
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::mission || request.command == helion::protocol::Command::accept || request.command == helion::protocol::Command::turnin) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::string response;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          if (request.command == helion::protocol::Command::mission) {
            response = profile.missionStage == 0 ? "MISSION 1 title=First Ore issuer=corp.orion issuer-name=Orion_Extraction_Group jurisdiction=authority.kepler jurisdiction-name=Kepler_Authority objective=mine-1 reward=250" :
              profile.missionStage == 1 ? "MISSION 1 active title=First Ore issuer=corp.orion issuer-name=Orion_Extraction_Group jurisdiction=authority.kepler jurisdiction-name=Kepler_Authority objective=mine-1 reward=250" :
              "MISSION 1 complete issuer=corp.orion issuer-name=Orion_Extraction_Group jurisdiction=authority.kepler jurisdiction-name=Kepler_Authority";
          } else if (request.command == helion::protocol::Command::accept) {
            if (profile.missionStage != 0) response = "ERR mission-unavailable";
            else {
              const auto before = profile;
              const bool dirtyBefore = flightStateDirty;
              const auto galnetBefore = galnetEvents;
              profile.missionStage = 1;
              profile.missionOreMined = false;
              try {
                addGalNetEvent("orion-first-ore-available", "Orion Extraction Group opened the First Ore contract under Kepler Authority jurisdiction.");
                persistStateLocked();
                response = "OK MISSION ACCEPTED id=1 issuer=corp.orion jurisdiction=authority.kepler";
              }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                galnetEvents = galnetBefore;
                response = "ERR persistence-failed";
                std::cerr << error.what() << '\n';
              }
            }
          } else if (profile.missionStage != 1) response = "ERR mission-not-active";
          else if (!profile.missionOreMined || !activeShip(profile).flight.docked) response = "ERR objective-incomplete";
          else if (profile.credits > std::numeric_limits<int>::max() - 250 ||
                   profile.experience > std::numeric_limits<int>::max() - 25) response = "ERR profile-limit";
          else {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            const auto galnetBefore = galnetEvents;
            const auto telemetryBefore = economicTelemetry;
            profile.missionStage = 2;
            profile.missionOreMined = false;
            profile.credits += 250;
            profile.experience += 25;
            try {
              helion::economy::recordCurrency(economicTelemetry, helion::economy::CurrencyFlow::faucet, 250);
              economicTelemetry.missionPayouts += 250;
              changeStanding(profile, "corp.orion", 10);
              changeStanding(profile, "authority.kepler", 5);
              addGalNetEvent("orion-first-ore-completed", "Orion Extraction Group reports a successful First Ore delivery in Kepler.");
              persistStateLocked();
              response = "OK MISSION COMPLETE reward=250 issuer=corp.orion jurisdiction=authority.kepler " +
                reputationLine(profile, "corp.orion", 10) + " " + reputationLine(profile, "authority.kepler", 5);
            }
            catch (const std::exception& error) {
              profile = before;
              flightStateDirty = dirtyBefore;
              galnetEvents = galnetBefore;
              economicTelemetry = telemetryBefore;
              response = "ERR persistence-failed";
              std::cerr << error.what() << '\n';
            }
          }
        }
        sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::fire ||
                 request.command == helion::protocol::Command::recover) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          const auto before = profile;
          const bool dirtyBefore = flightStateDirty;
          const auto galnetBefore = galnetEvents;
          const auto telemetryBefore = economicTelemetry;
          if (request.command == helion::protocol::Command::recover) {
            const auto result = helion::flight::recover(activeShip(profile).flight);
            if (result.rfind("OK", 0) == 0) {
              try {
                persistStateLocked();
                responses.push_back(result);
              } catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            } else responses.push_back(result);
          } else if (activeShip(profile).flight.docked) {
            responses.push_back("ERR launch-required");
          } else if (activeShip(profile).flight.destroyed) {
            responses.push_back("ERR recovery-required");
          } else {
            const auto* weapon = fittedModule(profile, helion::loadout::Slot::weapon);
            if (!weapon || weapon->weaponDamage <= 0 || weapon->weaponRange <= 0 || weapon->weaponCooldown <= 0)
              responses.push_back("ERR weapon-required");
            else if (request.first != profile.hostile.id) responses.push_back("ERR invalid-target");
            else if (profile.hostile.destroyed) responses.push_back("ERR target-destroyed");
            else if (helion::combat::distance(profile.hostile, activeShip(profile).flight) > weapon->weaponRange)
              responses.push_back("ERR target-out-of-range");
            else if (activeShip(profile).flight.weaponCooldown > 0) responses.push_back("ERR weapon-cooldown");
            else {
              activeShip(profile).flight.weaponCooldown = weapon->weaponCooldown;
              profile.hostile.hull = std::max(0, profile.hostile.hull - weapon->weaponDamage);
              responses.push_back("COMBAT HIT target=" + profile.hostile.id +
                " damage=" + std::to_string(weapon->weaponDamage) +
                " hull=" + std::to_string(profile.hostile.hull));
              if (profile.hostile.hull == 0) {
                profile.hostile.destroyed = true;
                profile.combatTargetDefeated = true;
                constexpr int kCredits = 100;
                constexpr int kExperience = 25;
                if (profile.credits > std::numeric_limits<int>::max() - kCredits ||
                    profile.experience > std::numeric_limits<int>::max() - kExperience ||
                    profile.salvage == std::numeric_limits<int>::max()) {
                  profile = before;
                  flightStateDirty = dirtyBefore;
                  responses.clear();
                  responses.push_back("ERR profile-limit");
                } else {
                  profile.credits += kCredits;
                  profile.experience += kExperience;
                  ++profile.salvage;
                  helion::economy::recordCurrency(economicTelemetry, helion::economy::CurrencyFlow::faucet, kCredits);
                  economicTelemetry.combatPayouts += kCredits;
                  responses.push_back("COMBAT DESTROYED target=" + profile.hostile.id);
                  responses.push_back("TRANSACTION COMBAT_REWARD target=" + profile.hostile.id +
                    " credits=" + std::to_string(kCredits) +
                    " experience=" + std::to_string(kExperience) + " salvage=1 affiliation=criminal.red_wake");
                  changeStanding(profile, "authority.kepler", 5);
                  changeStanding(profile, "criminal.red_wake", -10);
                  if (helion::career::defeated(profile.career)) {
                    responses.push_back("OK CAREER COMPLETE id=career.red_wake_response recognition=established-kepler-pilot");
                    responses.push_back(helion::career::status(profile.career, profile.missionStage));
                    addGalNetEvent("career-established-pilot", "Your Red Wake response is complete; Kepler recognizes an established pilot.");
                  }
                  addGalNetEvent("red-wake-raider-defeated", "A Red Wake raider was defeated in Kepler; local security credits the response.");
                  addGalNetEvent("kepler-security-response", "Kepler Authority security forces report a response to Red Wake activity.");
                  responses.push_back(reputationLine(profile, "authority.kepler", 5));
                  responses.push_back(reputationLine(profile, "criminal.red_wake", -10));
                }
              }
              if (responses.empty() || responses.back().line.rfind("ERR", 0) != 0) {
                try { persistStateLocked(); }
                catch (const std::exception& error) {
                  profile = before;
                  flightStateDirty = dirtyBefore;
                  galnetEvents = galnetBefore;
                  economicTelemetry = telemetryBefore;
                  responses.clear();
                  responses.push_back("ERR persistence-failed");
                  std::cerr << error.what() << '\n';
                }
              }
            }
          }
          responses.push_back(helion::flight::combatStatusLine(activeShip(profile).flight));
          appendFlightState(responses, profile);
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::shipyard) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          auto& current = activeShip(profile);
          if (!current.flight.docked || current.flight.destroyed) {
            responses.push_back("ERR dock-required");
          } else if (request.first == "LIST" || request.first == "OWNED") {
            appendShipyardState(responses, profile, request.first == "LIST", environmentPolicy.bypassShipRequirements(user));
          } else if (request.first == "BUY") {
            const auto* hull = helion::ships::find(request.second);
            const int station = current.flight.station;
            const int price = hull ? configuredPurchasePrice(hull->purchasePrice) : 0;
            if (!hull) responses.push_back("ERR unknown-hull");
            else if (!helion::shipyard::available(station,
              helion::flight::kStations[static_cast<std::size_t>(station)].maximumPad,
              hull->id, profile.reputation, environmentPolicy.bypassShipRequirements(user))) responses.push_back("ERR hull-unavailable");
            else if (std::any_of(profile.ownedShips.begin(), profile.ownedShips.end(),
              [hull](const auto& owned) { return owned.hullId == hull->id; }))
              responses.push_back("ERR ship-already-owned");
            else if (profile.credits < price) responses.push_back("ERR insufficient-credits");
            else {
              const auto before = profile;
              const bool dirtyBefore = flightStateDirty;
              const auto telemetryBefore = economicTelemetry;
              const std::string instance = helion::ships::deterministicInstanceId(user, profile.nextShipSequence++);
              profile.credits -= price;
              profile.ownedShips.push_back(helion::ships::makePurchasedShip(instance, *hull, station));
              helion::economy::recordCurrency(economicTelemetry, helion::economy::CurrencyFlow::sink, price);
              economicTelemetry.shipPurchaseValue += price;
              try {
                persistStateLocked();
                responses.push_back("OK SHIP PURCHASED instance=" + instance + " hull=" + std::string(hull->id) +
                  " cost=" + std::to_string(price));
                responses.push_back("TRANSACTION SHIP_PURCHASE instance=" + instance + " hull=" +
                  std::string(hull->id) + " station=" + std::to_string(station) +
                  " credits=" + std::to_string(price));
              } catch (const std::exception& error) {
                profile = before; flightStateDirty = dirtyBefore;
                economicTelemetry = telemetryBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            appendShipyardState(responses, profile, true, environmentPolicy.bypassShipRequirements(user));
          } else if (request.first == "SWITCH") {
            const auto target = std::find_if(profile.ownedShips.begin(), profile.ownedShips.end(),
              [&request](const auto& owned) { return owned.instanceId == request.second; });
            if (target == profile.ownedShips.end()) responses.push_back("ERR ship-not-owned");
            else if (target->instanceId == profile.activeShipId) responses.push_back("ERR ship-already-active");
            else if (!target->stored || !target->flight.docked || target->flight.destroyed ||
                     target->flight.station != current.flight.station) responses.push_back("ERR ship-not-at-station");
            else {
              const auto before = profile;
              const bool dirtyBefore = flightStateDirty;
              current.stored = true;
              target->stored = false;
              profile.activeShipId = target->instanceId;
              refreshDerivedShipState(profile);
              try {
                persistStateLocked();
                responses.push_back("OK SHIP ACTIVE instance=" + profile.activeShipId +
                  " hull=" + activeShip(profile).hullId);
              } catch (const std::exception& error) {
                profile = before; flightStateDirty = dirtyBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            appendShipyardState(responses, profile, true, environmentPolicy.bypassShipRequirements(user));
            appendFlightState(responses, profile);
          }
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::refuel ||
                 request.command == helion::protocol::Command::outfit) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          if (request.command == helion::protocol::Command::refuel) {
            if (!activeShip(profile).flight.docked) responses.push_back("ERR dock-required");
            else {
              const auto before = profile;
              const bool dirtyBefore = flightStateDirty;
              const auto telemetryBefore = economicTelemetry;
              helion::flight::FuelTransaction transaction;
              const auto result = helion::flight::refuel(activeShip(profile).flight, profile.credits, &transaction);
              if (result.rfind("OK", 0) == 0) {
                helion::economy::recordCurrency(economicTelemetry, helion::economy::CurrencyFlow::sink,
                                                transaction.creditsSpent);
                economicTelemetry.serviceValue += transaction.creditsSpent;
                try { persistStateLocked(); }
                catch (const std::exception& error) {
                  profile = before;
                  flightStateDirty = dirtyBefore;
                  economicTelemetry = telemetryBefore;
                  responses.push_back("ERR persistence-failed");
                  std::cerr << error.what() << '\n';
                }
              }
              if (responses.empty()) {
                responses.push_back(result);
                if (result.rfind("OK", 0) == 0)
                  responses.push_back(helion::flight::fuelTransactionLine(transaction));
              }
            }
            appendFlightState(responses, profile);
          } else if (request.first == "LIST") {
            appendOutfittingState(responses, profile);
          } else if (!activeShip(profile).flight.docked) {
            responses.push_back("ERR dock-required");
          } else {
            const auto* module = helion::loadout::find(request.second);
            helion::loadout::Slot slot = helion::loadout::Slot::mining;
            bool validSlot = true;
            if (request.first == "REMOVE") {
              if (request.second == "mining") slot = helion::loadout::Slot::mining;
              else if (request.second == "engine") slot = helion::loadout::Slot::engine;
              else if (request.second == "defense") slot = helion::loadout::Slot::defense;
              else if (request.second == "weapon") slot = helion::loadout::Slot::weapon;
              else validSlot = false;
              if (!validSlot) responses.push_back("ERR unknown-slot");
              else if (activeShip(profile).fittedModules[helion::loadout::slotIndex(slot)].empty()) responses.push_back("ERR slot-empty");
              else {
                const auto before = profile;
                const bool dirtyBefore = flightStateDirty;
                activeShip(profile).fittedModules[helion::loadout::slotIndex(slot)].clear();
                refreshDerivedShipState(profile);
                try {
                  persistStateLocked();
                  responses.push_back("OK MODULE REMOVED slot=" + request.second);
                } catch (const std::exception& error) {
                  profile = before;
                  flightStateDirty = dirtyBefore;
                  responses.push_back("ERR persistence-failed");
                  std::cerr << error.what() << '\n';
                }
              }
            } else if (!module) {
              responses.push_back("ERR unknown-module");
            } else if (request.first == "BUY") {
              const auto fit = helion::loadout::compatibility(*module,
                helion::ships::definition(activeShip(profile)));
              const int price = configuredPurchasePrice(module->purchasePrice, module->testAvailable);
              if (ownsModule(profile, request.second)) responses.push_back("ERR module-owned");
              else if (!fit.compatible) responses.push_back("ERR module-incompatible reason=" +
                std::string(helion::loadout::fitIssueName(fit.issue)));
              else if (profile.credits < price) responses.push_back("ERR insufficient-credits");
              else {
                const auto before = profile;
                const bool dirtyBefore = flightStateDirty;
                const auto telemetryBefore = economicTelemetry;
                profile.credits -= price;
                activeShip(profile).ownedModules.push_back(module->id);
                helion::economy::recordCurrency(economicTelemetry, helion::economy::CurrencyFlow::sink, price);
                economicTelemetry.modulePurchaseValue += price;
                try {
                  persistStateLocked();
                  responses.push_back("OK MODULE BOUGHT module=" + std::string(module->id) +
                                     " slot=" + helion::loadout::slotName(module->slot) +
                                     " cost=" + std::to_string(price));
                  responses.push_back("TRANSACTION MODULE_PURCHASE module=" + std::string(module->id) +
                                     " slot=" + helion::loadout::slotName(module->slot) +
                                     " credits=" + std::to_string(price));
                } catch (const std::exception& error) {
                  profile = before;
                  flightStateDirty = dirtyBefore;
                  economicTelemetry = telemetryBefore;
                  responses.push_back("ERR persistence-failed");
                  std::cerr << error.what() << '\n';
                }
              }
            } else if (request.first == "FIT") {
              const auto fit = helion::loadout::compatibility(*module,
                helion::ships::definition(activeShip(profile)));
              if (!ownsModule(profile, request.second)) responses.push_back("ERR module-not-owned");
              else if (!fit.compatible) responses.push_back("ERR module-incompatible reason=" +
                std::string(helion::loadout::fitIssueName(fit.issue)));
              else if (activeShip(profile).fittedModules[helion::loadout::slotIndex(module->slot)] == module->id)
                responses.push_back("ERR module-already-fitted");
              else {
                const auto before = profile;
                const bool dirtyBefore = flightStateDirty;
                activeShip(profile).fittedModules[helion::loadout::slotIndex(module->slot)] = module->id;
                refreshDerivedShipState(profile);
                try {
                  persistStateLocked();
                  responses.push_back("OK MODULE FIT module=" + std::string(module->id) +
                                     " slot=" + helion::loadout::slotName(module->slot));
                } catch (const std::exception& error) {
                  profile = before;
                  flightStateDirty = dirtyBefore;
                  responses.push_back("ERR persistence-failed");
                  std::cerr << error.what() << '\n';
                }
              }
            } else {
              responses.push_back("ERR invalid-outfit-action");
            }
            responses.push_back(loadoutLine(profile));
            appendFlightState(responses, profile);
          }
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::upgrade || request.command == helion::protocol::Command::repair) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          auto& owned = activeShip(profile);
          if (!owned.flight.docked) responses.push_back("ERR dock-required");
          else if (request.command == helion::protocol::Command::repair) {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            const auto telemetryBefore = economicTelemetry;
            const auto* defense = fittedModule(profile, helion::loadout::Slot::defense);
            const auto result = helion::flight::repairToCapacity(owned.flight, profile.credits,
              helion::ships::hullCapacity(owned, defense ? defense->hullBonus : 0));
            if (result.rfind("OK", 0) == 0) {
              const int spent = before.credits - profile.credits;
              helion::economy::recordCurrency(economicTelemetry, helion::economy::CurrencyFlow::sink, spent);
              economicTelemetry.serviceValue += spent;
              try { persistStateLocked(); }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                economicTelemetry = telemetryBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            if (responses.empty()) responses.push_back(result);
            appendFlightState(responses, profile);
          } else {
            int& level = request.first == "hull" ? owned.hullLevel : owned.engineLevel;
            const int cost = level * 500;
            if (level >= 5) responses.push_back("ERR upgrade-max");
            else if (profile.credits < cost) responses.push_back("ERR insufficient-credits");
            else {
              const auto before = profile;
              const bool dirtyBefore = flightStateDirty;
              const auto telemetryBefore = economicTelemetry;
              profile.credits -= cost;
              ++level;
              refreshDerivedShipState(profile);
              helion::economy::recordCurrency(economicTelemetry, helion::economy::CurrencyFlow::sink, cost);
              economicTelemetry.serviceValue += cost;
              try {
                persistStateLocked();
                responses.push_back("OK UPGRADE " + request.first + " level=" + std::to_string(level) + " cost=" + std::to_string(cost));
              } catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                economicTelemetry = telemetryBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            appendFlightState(responses, profile);
          }
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::contacts) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          for (const auto& contact : contactsFor(user)) responses.push_back(helion::flight::contactLine(contact));
          responses.push_back("CONTACTS END");
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::buy || request.command == helion::protocol::Command::sell) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          int quantity = 0;
          try { quantity = std::stoi(request.second); }
          catch (...) { responses.push_back("ERR invalid-quantity"); }
          if (responses.empty()) {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            const auto marketsBefore = regionalMarkets;
            const auto telemetryBefore = economicTelemetry;
            auto& owned = activeShip(profile);
            const auto transaction = helion::economy::trade(regionalMarkets, owned.flight, profile.credits,
              request.command == helion::protocol::Command::buy, request.first, quantity,
              helion::ships::cargoCapacity(owned), pricingPolicy());
            if (transaction.committed) {
              if (request.command == helion::protocol::Command::buy)
                helion::career::purchased(profile.career, owned.flight.station, request.first, quantity);
              helion::economy::recordMarketTrade(economicTelemetry, transaction);
              try { persistStateLocked(); }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                regionalMarkets = marketsBefore;
                economicTelemetry = telemetryBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            if (responses.empty()) responses.push_back(transaction.line);
            appendFlightState(responses, profile);
          }
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::launch ||
                 request.command == helion::protocol::Command::input ||
                 request.command == helion::protocol::Command::flight ||
                 request.command == helion::protocol::Command::mine ||
                 request.command == helion::protocol::Command::dock) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<OutboundResponse> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          auto& owned = activeShip(profile);
          auto& flight = owned.flight;
          if (flight.destroyed && request.command != helion::protocol::Command::flight) {
            responses.push_back("ERR recovery-required");
          } else if (request.command == helion::protocol::Command::input) {
            flight.input = {request.thrust, request.turn, request.brake};
            flight.inputAge = 0;
            flightStateDirty = true;
          } else if (request.command != helion::protocol::Command::flight) {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            const auto galnetBefore = galnetEvents;
            const auto marketsBefore = regionalMarkets;
            const auto telemetryBefore = economicTelemetry;
            const bool docking = request.command == helion::protocol::Command::dock;
            helion::flight::DockTransaction transaction;
            std::string result;
            if (request.command == helion::protocol::Command::launch) result = helion::flight::launch(flight);
            else if (request.command == helion::protocol::Command::mine) {
              const auto* mining = fittedModule(profile, helion::loadout::Slot::mining);
              result = mining && helion::ships::definition(owned).miningCapability ?
                helion::flight::mine(flight, helion::ships::cargoCapacity(owned), true, mining->miningCooldownMultiplier) :
                "ERR mining-module-required";
            } else {
              const auto sale = helion::economy::dockAndSellOre(regionalMarkets, flight, profile.credits,
                                                                profile.experience, pricingPolicy());
              result = sale.line;
              transaction = sale.transaction;
            }
            if (result.rfind("OK", 0) == 0) {
              if (request.command == helion::protocol::Command::launch) {
                resetCombatAfterLaunch(profile);
                addGalNetEvent("red-wake-kepler-activity", "Kepler Authority warns of Red Wake activity in the mining sector.");
              }
              if (request.command == helion::protocol::Command::mine) {
                ++economicTelemetry.oreMined;
                if (profile.missionStage == 1) profile.missionOreMined = true;
              }
              if (docking) helion::economy::recordOreSale(economicTelemetry, transaction);
              try { persistStateLocked(); }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                galnetEvents = galnetBefore;
                regionalMarkets = marketsBefore;
                economicTelemetry = telemetryBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            if (responses.empty()) {
              responses.push_back(result);
              if (docking && result.rfind("OK", 0) == 0)
                responses.push_back(helion::flight::dockTransactionLine(transaction));
            }
          }
          appendFlightState(responses, profile);
        }
        for (const auto& response : responses) sendResponse(fd, response);
      } else if (request.command == helion::protocol::Command::state) {
        sendLine(fd, stateLine());
      } else if (request.command == helion::protocol::Command::quit) {
        sendLine(fd, "OK BYE");
        running = false;
        break;
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--hash-password") {
    std::string password;
    if (!std::getline(std::cin, password) || password.size() < helion::security::kMinNewPassword ||
        password.size() > helion::security::kMaxPassword) return 2;
    try { std::cout << helion::security::hashPassword(password) << '\n'; return 0; }
    catch (...) { return 1; }
  }
  std::signal(SIGPIPE, SIG_IGN);
  std::signal(SIGINT, stopSignal);
  std::signal(SIGTERM, stopSignal);
  std::signal(SIGUSR1, maintenanceSignal);
  int port = kDefaultPort;
  std::size_t maxClients = kDefaultMaxClients;
  std::string bindAddress = "127.0.0.1";
  std::string certificate, privateKey;
  int positional = 0;
  bool economySelected = false;
  std::string recoveryAction, recoveryId, recoveryOperator;
  std::string operationAction, operationId, operationReason;
  unsigned drainSeconds = 1, warningSeconds = 60;
  bool drainSelected = false, warningSelected = false;
  if (!helion::ships::validateRegistry() || !helion::loadout::validateRegistry() ||
      !helion::galaxy::validateGalaxy(helion::galaxy::seedState())) {
    std::cerr << "invalid canonical ship, module, or galaxy registry\n";
    return 1;
  }
  auto number = [](const std::string& value, unsigned long maximum) -> unsigned long {
    std::size_t end = 0;
    const unsigned long parsed = std::stoul(value, &end);
    if (end != value.size() || parsed == 0 || parsed > maximum) throw std::invalid_argument("invalid number");
    return parsed;
  };
  try {
    std::set<std::string> selectedOptions;
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg.rfind("--", 0) == 0 && !selectedOptions.insert(arg).second)
        throw std::invalid_argument("repeated startup option");
      if (arg == "--bind" && i + 1 < argc) bindAddress = argv[++i];
      else if (arg == "--cert" && i + 1 < argc) certificate = argv[++i];
      else if (arg == "--key" && i + 1 < argc) privateKey = argv[++i];
      else if (arg == "--max-clients" && i + 1 < argc) maxClients = number(argv[++i], 1024);
      else if (arg == "--drain-seconds" && i + 1 < argc && !drainSelected) {
        drainSeconds = static_cast<unsigned>(number(argv[++i], 5)); drainSelected = true;
      }
      else if (arg == "--maintenance-warning-seconds" && i + 1 < argc && !warningSelected) {
        warningSeconds = static_cast<unsigned>(number(argv[++i], 86400)); warningSelected = true;
      }
      else if (arg == "--operations" && i + 3 < argc && operationAction.empty()) {
        if (std::string(argv[++i]) != "PRODUCTION") throw std::invalid_argument("explicit PRODUCTION target required");
        operationAction = argv[++i]; operationId = argv[++i];
        if ((operationAction != "BACKUP" && operationAction != "VERIFY_BACKUP" && operationAction != "PLAN_RESTORE") ||
            !helion::server::safeIdentifier(operationId, 80)) throw std::invalid_argument("invalid operation");
      }
      else if (arg == "--reason" && i + 1 < argc && operationReason.empty()) {
        operationReason = argv[++i];
        if (!helion::server::safeIdentifier(operationReason, 96)) throw std::invalid_argument("invalid operational reason");
      }
      else if (arg == "--environment" && i + 1 < argc) {
        const auto parsed = helion::server::parseRuntimeEnvironment(argv[++i]);
        if (!parsed || environmentPolicy.explicitEnvironment) throw std::invalid_argument("unknown or repeated environment");
        environmentPolicy.explicitEnvironment = true;
        runtimeEnvironment = *parsed;
      }
      else if (arg == "--access-file" && i + 1 < argc && accessFile.empty()) accessFile = argv[++i];
      else if (arg == "--test-economy" && i + 1 < argc && !economySelected) {
        const std::string value = argv[++i];
        if (value != "convenience" && value != "production-like") throw std::invalid_argument("invalid economy policy");
        environmentPolicy.convenienceEconomy = value == "convenience";
        economySelected = true;
      }
      else if (arg == "--test-recovery" && i + 3 < argc && recoveryAction.empty()) {
        if (std::string(argv[++i]) != "TEST") throw std::invalid_argument("explicit TEST target required");
        recoveryAction = argv[++i]; recoveryId = argv[++i];
        if ((recoveryAction != "SNAPSHOT" && recoveryAction != "RESTORE" && recoveryAction != "RESET") ||
            !helion::server::safeIdentifier(recoveryId)) throw std::invalid_argument("invalid recovery operation");
      }
      else if (arg == "--operator" && i + 1 < argc && recoveryOperator.empty()) recoveryOperator = argv[++i];
      else if (arg.rfind("--", 0) == 0) throw std::invalid_argument("unknown option");
      else if (positional++ == 0) port = static_cast<int>(number(arg, 65535));
      else if (positional == 2) dataPath = arg;
      else throw std::invalid_argument("too many arguments");
    }
  } catch (const std::exception&) {
    helion::server::operationLog(environmentPolicy, "configuration", "invalid");
    std::cerr << "usage: helion_server [port] [data-file] [--environment development|test|production] [--bind IPv4-address] [--max-clients 1..1024] --cert certificate.pem --key private-key.pem\n";
    return 2;
  }
  if ((!recoveryOperator.empty() && recoveryAction.empty()) ||
      ((economySelected || !recoveryAction.empty()) && runtimeEnvironment != helion::server::RuntimeEnvironment::private_test)) {
    std::cerr << "TEST-only startup option rejected\n"; return 2;
  }

  if ((!operationAction.empty() && (runtimeEnvironment != helion::server::RuntimeEnvironment::production ||
        !environmentPolicy.explicitEnvironment || !recoveryAction.empty() || operationReason.empty())) ||
      (!operationReason.empty() && operationAction.empty())) {
    std::cerr << "explicit Production operation/environment/reason required\n"; return 2;
  }
  helion::server::operationLog(environmentPolicy, "startup", "validating");
  if (runtimeEnvironment == helion::server::RuntimeEnvironment::production &&
      (!std::filesystem::path(dataPath).is_absolute() || positional != 2)) {
    helion::server::operationLog(environmentPolicy, "configuration", "fatal");
    std::cerr << "Production requires an explicit absolute persistence path\n"; return 2;
  }
  try { if (!accessFile.empty()) environmentPolicy.loadAccessFile(accessFile); }
  catch (const std::exception& error) { helion::server::operationLog(environmentPolicy, "startup", "fatal"); std::cerr << error.what() << '\n'; return 1; }
  regionalMarkets = helion::economy::seedMarkets(pricingPolicy());
  galaxyState = helion::galaxy::seedState();
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, bindAddress.c_str(), &address.sin_addr) != 1) {
    std::cerr << "--bind requires a numeric IPv4 address\n";
    return 2;
  }
  helion::tls::Context tlsContext(nullptr, SSL_CTX_free);
  try {
    // Validate storage/credential isolation before TLS reads or persistence load.
    environmentPolicy.protectStorage(dataPath, accessFile, certificate, privateKey, false);
    if (runtimeEnvironment == helion::server::RuntimeEnvironment::production) {
      (void)helion::server::readPrivateFile(privateKey, 65536);
    }
    tlsContext = helion::tls::serverContext(certificate, privateKey);
    if (runtimeEnvironment == helion::server::RuntimeEnvironment::production) {
      const auto* cert = SSL_CTX_get0_certificate(tlsContext.get());
      if (!cert || X509_cmp_current_time(X509_get0_notBefore(cert)) >= 0 ||
          X509_cmp_current_time(X509_get0_notAfter(cert)) <= 0)
        throw std::runtime_error("Production certificate validity check failed");
    }
    helion::server::operationLog(environmentPolicy, "tls", "initialized");
  }
  catch (const std::exception& error) { helion::server::operationLog(environmentPolicy, "startup", "fatal"); std::cerr << error.what() << '\n'; return 1; }
  const auto loadStart = std::chrono::steady_clock::now();
  std::unique_ptr<helion::server::DataLock> dataLock;
  try {
    dataLock = std::make_unique<helion::server::DataLock>(dataPath);
    std::lock_guard<std::mutex> lock(stateMutex);
    helion::server::publishStatus(dataPath, environmentPolicy, helion::server::ServicePhase::loading,
      serviceMetrics, 0, 0, 0, false);
    dummyPasswordHash = helion::security::hashPassword(std::string(12, 'x'));
    helion::server::operationLog(environmentPolicy, "persistence-load", "loading");
    // Verification can diagnose backups even when the live file is damaged.
    if (operationAction.empty() || operationAction == "BACKUP") loadState();
    if (operationAction.empty() || operationAction == "BACKUP")
      environmentPolicy.protectStorage(dataPath, accessFile, certificate, privateKey);
    if (!operationAction.empty()) return performProductionOperation(operationAction, operationId, operationReason);
    migrateLegacyProfiles();
    if (serviceMetrics.storageFatal) throw std::runtime_error("startup persistence durability failed");
    serviceMetrics.loadMicros = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - loadStart).count();
    helion::server::operationLog(environmentPolicy, "persistence-load", "ready");
    if (!recoveryAction.empty()) return performTestRecovery(recoveryAction, recoveryId, recoveryOperator);
  } catch (const std::exception& error) {
    helion::server::operationLog(environmentPolicy, "startup", "fatal");
    if (dataLock) {
      try { helion::server::publishStatus(dataPath, environmentPolicy, helion::server::ServicePhase::fatal,
        serviceMetrics, 0, 0, 0, false); } catch (...) {}
    }
    std::cerr << "unable to load or securely migrate persistence: " << error.what() << '\n';
    return 1;
  }
  const int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) { helion::server::operationLog(environmentPolicy, "listener", "fatal"); std::perror("socket"); return 1; }
  int yes = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
      listen(server, static_cast<int>(maxClients)) < 0) {
    helion::server::operationLog(environmentPolicy, "listener", "fatal");
    std::perror("bind/listen");
    close(server);
    return 1;
  }
  std::cout << "Helion server listening on " << bindAddress << ':' << port
            << " TLS enabled; environment=" << helion::server::runtimeEnvironmentName(runtimeEnvironment)
            << "; channel=" << environmentPolicy.releaseChannel() << "; instance=" << environmentPolicy.instance
            << "; max-clients=" << maxClients << '\n';
  helion::server::ConnectionLimit limit(maxClients);
  struct Worker { std::thread thread; std::shared_ptr<std::atomic<bool>> done; };
  std::vector<Worker> workers;
  workers.reserve(maxClients + 1);
  auto reap = [&workers]() {
    for (auto it = workers.begin(); it != workers.end();) {
      if (it->done->load()) {
        it->thread.join();
        it = workers.erase(it);
      } else ++it;
    }
  };
  auto lastTick = std::chrono::steady_clock::now();
  auto nextCheckpoint = lastTick + std::chrono::duration<double>(kFlightCheckpointSeconds);
  auto nextMarketRestock = lastTick + std::chrono::duration<double>(kMarketRestockSeconds);
  double accumulator = 0;
  helion::server::ServiceLifecycle lifecycle;
  const auto serviceStart = lastTick;
  lifecycle.online();
  bool operationalFailure = false;
  bool healthPublicationHealthy = true;
  auto reportStatus = [&](helion::server::ServicePhase phase) {
    try {
      std::size_t players = 0;
      { std::lock_guard<std::mutex> lock(stateMutex); players = authenticatedClients.size(); }
      const bool durable = serviceMetrics.persistenceHealthy && !serviceMetrics.storageFatal;
      if (!durable && phase == helion::server::ServicePhase::online) phase = helion::server::ServicePhase::degraded;
      helion::server::publishStatus(dataPath, environmentPolicy, phase, serviceMetrics,
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - serviceStart).count(),
        limit.active(), players, durable);
      healthPublicationHealthy = true;
      return true;
    } catch (...) {
      if (healthPublicationHealthy)
        helion::server::operationLog(environmentPolicy, "health-publication", "failed");
      healthPublicationHealthy = false;
      return false;
    }
  };
  helion::server::operationLog(environmentPolicy, "service", "ready");
  reportStatus(lifecycle.phase);
  auto nextStatus = serviceStart + std::chrono::milliseconds(500);
  while (true) {
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - serviceStart).count();
    const auto previousPhase = lifecycle.phase;
    if (maintenanceRequested) { lifecycle.maintenance(seconds, warningSeconds); maintenanceRequested = 0; }
    if (stopping || serviceMetrics.storageFatal) lifecycle.drain(seconds, drainSeconds);
    const bool drained = lifecycle.advance(seconds, drainSeconds);
    draining = lifecycle.phase == helion::server::ServicePhase::draining;
    if (lifecycle.phase != previousPhase) {
      helion::server::operationLog(environmentPolicy, "lifecycle", helion::server::phaseName(lifecycle.phase));
      reportStatus(lifecycle.phase);
    }
    if (drained || (draining.load() && limit.active() == 0)) break;
    if (now >= nextStatus) {
      reportStatus(lifecycle.phase);
      nextStatus = now + std::chrono::milliseconds(500);
    }
    accumulator += std::min(0.1, std::chrono::duration<double>(now - lastTick).count());
    lastTick = now;
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      while (accumulator >= 1.0 / 60) {
        for (auto& entry : profiles) {
          auto& owned = activeShip(entry.second);
          const auto before = owned.flight;
          const auto* engine = fittedModule(entry.second, helion::loadout::Slot::engine);
          const double fuelMultiplier = engine ? engine->fuelConsumptionMultiplier : 1.0;
          helion::flight::step(owned.flight, 1.0 / 60, helion::ships::definition(owned), owned.engineLevel, fuelMultiplier);
          if (!before.destroyed && !before.docked && owned.flight.destroyed && owned.flight.hull == 0)
            entry.second.combatEvents.push_back("COMBAT DESTROYED player=1 cargo-lost=1 recovery=RECOVER");
          helion::combat::step(entry.second.hostile, owned.flight, 1.0 / 60);
          if (helion::combat::canNpcFire(entry.second.hostile, owned.flight)) {
            entry.second.hostile.fireCooldown = helion::combat::kNpcFireCooldown;
            applyNpcDamage(entry.second);
          }
          if (!owned.flight.docked || before.docked != owned.flight.docked ||
              before.hull != owned.flight.hull || before.cargo != owned.flight.cargo ||
              before.fuel != owned.flight.fuel || before.destroyed != owned.flight.destroyed) {
            flightStateDirty = true;
          }
        }
        accumulator -= 1.0 / 60;
      }
    }
    const auto checkpointNow = std::chrono::steady_clock::now();
    if (checkpointNow >= nextCheckpoint) {
      std::lock_guard<std::mutex> lock(stateMutex);
      if (flightStateDirty) {
        try { persistStateLocked(); }
        catch (const std::exception& error) { std::cerr << "flight checkpoint failed: " << error.what() << '\n'; }
      }
      nextCheckpoint = checkpointNow + std::chrono::duration<double>(kFlightCheckpointSeconds);
    }
    if (checkpointNow >= nextMarketRestock) {
      std::lock_guard<std::mutex> lock(stateMutex);
      const auto marketsBefore = regionalMarkets;
      const auto telemetryBefore = economicTelemetry;
      const auto result = helion::economy::restock(regionalMarkets, pricingPolicy());
      const bool marketsChanged = !std::equal(
        regionalMarkets.begin(), regionalMarkets.end(), marketsBefore.begin(),
        [](const auto& left, const auto& right) {
          return left.station == right.station && left.commodityId == right.commodityId &&
            left.stock == right.stock && left.demand == right.demand && left.buyBudget == right.buyBudget;
        });
      if (marketsChanged || result.unitsAdded || result.demandAdded || result.budgetAdded) {
        helion::economy::recordRestock(economicTelemetry, result);
        try { persistStateLocked(); }
        catch (const std::exception& error) {
          regionalMarkets = marketsBefore;
          economicTelemetry = telemetryBefore;
          std::cerr << "market restock checkpoint failed: " << error.what() << '\n';
        }
      }
      nextMarketRestock = checkpointNow + std::chrono::duration<double>(kMarketRestockSeconds);
    }
    reap();
    pollfd ready{server, POLLIN, 0};
    const int polled = poll(&ready, 1, 16);
    if (polled < 0 && errno == EINTR) continue;
    if (polled < 0) { operationalFailure = true; std::perror("poll"); break; }
    if (polled == 0 || !(ready.revents & POLLIN)) continue;
    sockaddr_in peer{};
    socklen_t length = sizeof(peer);
    const int client = accept(server, reinterpret_cast<sockaddr*>(&peer), &length);
    if (client < 0) {
      if (errno == EINTR) continue;
      operationalFailure = true;
      std::perror("accept");
      break;
    }
    if (draining.load() || !healthPublicationHealthy || !serviceMetrics.persistenceHealthy || serviceMetrics.storageFatal) {
      ++serviceMetrics.rejected;
      close(client);
      continue;
    }
    timeval readTimeout{kReadTimeoutSeconds, 0};
    timeval writeTimeout{kWriteTimeoutSeconds, 0};
    if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &readTimeout, sizeof(readTimeout)) != 0 ||
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &writeTimeout, sizeof(writeTimeout)) != 0) {
      close(client);
      continue;
    }
    if (!limit.tryAcquire()) {
      ++serviceMetrics.rejected;
      // Refuse overload before a handshake; never emit plaintext on the TLS port.
      close(client);
      continue;
    }
    ++serviceMetrics.connections;
    helion::server::operationLog(environmentPolicy, "connection", "accepted");
    try {
      auto connection = std::make_shared<helion::tls::Connection>(tlsContext.get(), client);
      auto transport = std::make_shared<Transport>(connection);
      {
        std::lock_guard<std::mutex> lock(transportMutex);
        transports.emplace(client, transport);
      }
      auto done = std::make_shared<std::atomic<bool>>(false);
      workers.push_back({std::thread([client, transport, done, &limit]() {
        try {
          transport->connection->handshake(true);
          clientLoop(client, *transport->connection);
          std::lock_guard<std::mutex> lock(transport->writeMutex);
          transport->connection->closeNotify();
        } catch (const std::exception& error) {
          std::cerr << "client connection rejected: " << error.what() << '\n';
        }
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          if (flightStateDirty) {
            try { persistStateLocked(); }
            catch (const std::exception& error) {
              std::cerr << "disconnect flight checkpoint failed: " << error.what() << '\n';
            }
          }
        }
        removeClient(client);
        helion::server::operationLog(environmentPolicy, "connection", "closed");
        {
          std::lock_guard<std::mutex> lock(transportMutex);
          transports.erase(client);
        }
        shutdown(client, SHUT_RDWR);
        close(client);
        done->store(true);
        limit.release();
      }), done});
    } catch (const std::exception&) {
      { std::lock_guard<std::mutex> lock(transportMutex); transports.erase(client); }
      close(client);
      limit.release();
    }
  }
  draining = true;
  close(server);
  lifecycle.phase = helion::server::ServicePhase::shutting_down;
  reportStatus(lifecycle.phase);
  helion::server::operationLog(environmentPolicy, "drain", "closing-connections");
  {
    std::lock_guard<std::mutex> lock(transportMutex);
    for (const auto& entry : transports) shutdown(entry.first, SHUT_RDWR);
  }
  for (auto& worker : workers) worker.thread.join();
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    try {
      // Always take a final authoritative checkpoint, including zero-player service.
      persistStateLocked();
      if (serviceMetrics.storageFatal) throw std::runtime_error("uncertain persistence durability");
      if (runtimeEnvironment == helion::server::RuntimeEnvironment::production) {
        try {
          helion::server::createBackup(dataPath, environmentPolicy, helion::server::uniqueBackupId("shutdown"));
          ++serviceMetrics.backups; serviceMetrics.lastBackup = std::time(nullptr);
        } catch (...) { ++serviceMetrics.backupFailures; throw; }
      }
    } catch (const std::exception& error) {
      operationalFailure = true;
      std::cerr << "shutdown checkpoint/backup failed: " << error.what() << '\n';
    }
  }
  lifecycle.phase = operationalFailure || serviceMetrics.storageFatal ?
    helion::server::ServicePhase::fatal : helion::server::ServicePhase::offline;
  if (!reportStatus(lifecycle.phase)) operationalFailure = true;
  helion::server::operationLog(environmentPolicy, "shutdown", operationalFailure ? "failed" : "complete");
  return operationalFailure ? 1 : 0;
}
