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
#include <fcntl.h>
#include <iostream>
#include <iomanip>
#include <memory>
#include <limits>
#include <map>
#include <mutex>
#include <netinet/in.h>
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
#include "shared/flight.h"
#include "shared/combat.h"
#include "shared/career.h"
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
constexpr double kFlightCheckpointSeconds = 1.0;
std::string dataPath = "helion-server.db";
std::string dummyPasswordHash;
volatile std::sig_atomic_t stopping = 0;

void stopSignal(int) { stopping = 1; }

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

std::mutex stateMutex;
std::unordered_map<std::string, Profile> profiles;
std::vector<ChatMessage> messages;
std::vector<GalNetEvent> galnetEvents;
std::vector<int> clients;
bool flightStateDirty = false;
bool ownershipMigrationPending = false;
std::mutex transportMutex;
struct Transport {
  explicit Transport(std::shared_ptr<helion::tls::Connection> value) : connection(std::move(value)) {}
  std::shared_ptr<helion::tls::Connection> connection;
  std::mutex writeMutex;
};
std::unordered_map<int, std::shared_ptr<Transport>> transports;

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
    " price=" + std::to_string(hull.purchasePrice) + " mass=" + std::to_string(static_cast<int>(hull.mass)) +
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

void appendShipyardState(std::vector<std::string>& responses, const Profile& profile, bool includeOffers = true) {
  const auto& current = activeShip(profile);
  const int station = current.flight.station;
  responses.push_back("SHIPYARD BEGIN station=" + std::to_string(station));
  if (includeOffers) {
    for (const auto* hull : helion::shipyard::availableAt(station,
      helion::flight::kStations[static_cast<std::size_t>(station)].maximumPad, profile.reputation)) {
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
  std::ostringstream snapshot;
  snapshot << std::setprecision(17);
  for (const auto& [name, profile] : profiles) {
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
  }
  for (const auto& event : galnetEvents)
    snapshot << "G\t" << encode(event.id) << '\t' << encode(event.headline) << '\n';
  for (const auto& message : messages) {
    snapshot << "M\t" << encode(message.user) << '\t' << encode(message.text) << '\n';
  }
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
  const int fd = open(dataPath.c_str(), O_RDONLY | O_NOFOLLOW);
  if (fd < 0) {
    if (errno == ENOENT) return;
    throw std::runtime_error("cannot open persistence file");
  }
  struct stat info{};
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1 ||
      fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
    close(fd);
    throw std::runtime_error("cannot protect persistence file");
  }
  FILE* file = fdopen(fd, "r");
  if (!file) { close(fd); throw std::runtime_error("cannot read persistence file"); }
  char* buffer = nullptr;
  std::size_t capacity = 0;
  std::set<std::string> profilesWithShipRecords;
  std::set<std::string> instanceIds;
  try {
    ssize_t length;
    while ((length = ::getline(&buffer, &capacity, file)) >= 0) {
      std::string line(buffer, static_cast<std::size_t>(length));
      if (!line.empty() && line.back() == '\n') line.pop_back();
      if (!line.empty() && line.back() == '\r') line.pop_back();
      std::istringstream input(line);
      std::string kind, first, second, third;
      std::getline(input, kind, '\t');
      std::getline(input, first, '\t');
      std::getline(input, second, '\t');
      std::getline(input, third, '\t');
      if ((kind == "P" || kind == "H") && !first.empty() && !second.empty() && !third.empty()) {
        std::string faction, ship, creditsText, experienceText;
        std::getline(input, faction, '\t');
        std::getline(input, ship, '\t');
        std::getline(input, creditsText, '\t');
        std::getline(input, experienceText, '\t');
        Profile profile;
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
    instanceIds.clear();
    for (auto& [name, profile] : profiles) {
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
  bool changed = ownershipMigrationPending;
  for (auto& [name, profile] : migrated) {
    (void)name;
    if (helion::security::migrateLegacyCredential(profile.passwordHash, profile.legacyCredential)) {
      profile.legacyCredential = false;
      changed = true;
    }
  }
  if (!changed) return;
  profiles.swap(migrated);
  try { saveStateLocked(); ownershipMigrationPending = false; }
  catch (...) { profiles.swap(migrated); throw; }
  for (auto& [name, profile] : migrated) {
    (void)name;
    std::fill(profile.passwordHash.begin(), profile.passwordHash.end(), '\0');
  }
}

bool sendLine(int fd, const std::string& line) {
  if (!helion::protocol::validWireLine(line)) return false;
  std::shared_ptr<Transport> transport;
  {
    std::lock_guard<std::mutex> lock(transportMutex);
    const auto found = transports.find(fd);
    if (found == transports.end()) return false;
    transport = found->second;
  }
  std::lock_guard<std::mutex> lock(transport->writeMutex);
  return transport->connection->sendAll(line + "\n");
}

std::string stateLine() {
  std::lock_guard<std::mutex> lock(stateMutex);
  std::ostringstream out;
  out << "STATE profiles=" << profiles.size() << " messages=" << messages.size();
  return out.str();
}

void appendFlightState(std::vector<std::string>& responses, Profile& profile) {
  const auto& ship = activeShip(profile);
  const auto& hull = helion::ships::definition(ship);
  responses.push_back(helion::flight::snapshot(ship.flight, profile.credits, profile.experience));
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
  std::vector<int> recipients;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    recipients = clients;
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
  for (auto it = clients.begin(); it != clients.end(); ++it) {
    if (*it == fd) {
      clients.erase(it);
      break;
    }
  }
}

void clientLoop(int fd, helion::tls::Connection& connection) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    clients.push_back(fd);
  }
  sendLine(fd, helion::protocol::welcomeLine());
  sendLine(fd, "INFO commands=CREATE LOGIN CHAT PROFILE STATE GALNET CONTACTS BUY SELL MISSION ACCEPT TURNIN CAREER UPGRADE REPAIR REFUEL OUTFIT SHIPYARD FIRE RECOVER LAUNCH INPUT FLIGHT MINE DOCK QUIT");

  std::string user;
  int authFailures = 0;
  int authRequests = 0;
  helion::protocol::LineDecoder decoder;
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
    for (const auto& frame : decoder.feed(std::string_view(bytes, static_cast<std::size_t>(received)))) {
      lastCompleteLine = now;
      if (frame.kind != helion::protocol::FrameKind::line) {
        sendLine(fd, frame.kind == helion::protocol::FrameKind::too_long ? "ERR line-too-long" : "ERR malformed-message");
        continue;
      }
      const auto request = helion::protocol::parseRequest(frame.line);
      const bool authRequest = request.command == helion::protocol::Command::create ||
        request.command == helion::protocol::Command::login ||
        frame.line.rfind("CREATE", 0) == 0 || frame.line.rfind("LOGIN", 0) == 0;
      if (authRequest && ++authRequests > kMaxAuthRequests) {
        sendLine(fd, "ERR too-many-auth-attempts");
        running = false;
        break;
      }
      if (request.command == helion::protocol::Command::invalid) {
        sendLine(fd, "ERR " + request.error);
        continue;
      }
      if (request.command == helion::protocol::Command::create) {
        const auto& name = request.first;
        const auto& password = request.second;
        const auto& display = request.payload;
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
            profile.passwordHash = encoded;
            profile.display = display;
            addStarterShip(profile, name);
            profiles.emplace(name, std::move(profile));
            try {
              persistStateLocked();
              response = "OK CREATED user=" + name + " display=" + display;
              created = true;
            } catch (const std::exception& error) {
              profiles.erase(name);
              response = "ERR persistence-failed";
              std::cerr << error.what() << '\n';
            }
          }
        }
        if (created) user = name;
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
        if (!found || !authenticated) {
          sendLine(fd, "ERR invalid-login");
          if (++authFailures >= kMaxAuthFailures) { sendLine(fd, "ERR too-many-auth-attempts"); running = false; break; }
        } else {
          user = name;
          sendLine(fd, "OK LOGIN user=" + name + " display=" + display);
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
            if (it == profiles.end()) response = "ERR profile-missing";
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
                " " + loadoutLine(profile);
            }
          }
          sendLine(fd, response);
        }
      } else if (request.command == helion::protocol::Command::galnet) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          for (const auto& event : galnetEvents)
            responses.push_back("GALNET id=" + event.id + " headline=" + event.headline);
          responses.push_back("GALNET END");
        }
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::career) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          if (!request.first.empty()) {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            const auto galnetBefore = galnetEvents;
            const auto result = helion::career::act(profile.career, profile.missionStage, activeShip(profile).flight,
              profile.credits, profile.experience, fittedModule(profile, helion::loadout::Slot::weapon) != nullptr,
              request.first, request.second);
            if (result.line.rfind("OK", 0) == 0) {
              try {
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
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            } else responses.push_back(result.line);
          }
          responses.push_back(helion::career::status(profile.career, profile.missionStage));
          appendFlightState(responses, profile);
        }
        for (const auto& line : responses) sendLine(fd, line);
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
            profile.missionStage = 2;
            profile.missionOreMined = false;
            profile.credits += 250;
            profile.experience += 25;
            try {
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
              response = "ERR persistence-failed";
              std::cerr << error.what() << '\n';
            }
          }
        }
        sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::fire ||
                 request.command == helion::protocol::Command::recover) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          const auto before = profile;
          const bool dirtyBefore = flightStateDirty;
          const auto galnetBefore = galnetEvents;
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
              if (responses.empty() || responses.back().rfind("ERR", 0) != 0) {
                try { persistStateLocked(); }
                catch (const std::exception& error) {
                  profile = before;
                  flightStateDirty = dirtyBefore;
                  galnetEvents = galnetBefore;
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
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::shipyard) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          auto& current = activeShip(profile);
          if (!current.flight.docked || current.flight.destroyed) {
            responses.push_back("ERR dock-required");
          } else if (request.first == "LIST" || request.first == "OWNED") {
            appendShipyardState(responses, profile, request.first == "LIST");
          } else if (request.first == "BUY") {
            const auto* hull = helion::ships::find(request.second);
            const int station = current.flight.station;
            if (!hull) responses.push_back("ERR unknown-hull");
            else if (!helion::shipyard::available(station,
              helion::flight::kStations[static_cast<std::size_t>(station)].maximumPad,
              hull->id, profile.reputation)) responses.push_back("ERR hull-unavailable");
            else if (std::any_of(profile.ownedShips.begin(), profile.ownedShips.end(),
              [hull](const auto& owned) { return owned.hullId == hull->id; }))
              responses.push_back("ERR ship-already-owned");
            else if (profile.credits < hull->purchasePrice) responses.push_back("ERR insufficient-credits");
            else {
              const auto before = profile;
              const bool dirtyBefore = flightStateDirty;
              const std::string instance = helion::ships::deterministicInstanceId(user, profile.nextShipSequence++);
              profile.credits -= hull->purchasePrice;
              profile.ownedShips.push_back(helion::ships::makePurchasedShip(instance, *hull, station));
              try {
                persistStateLocked();
                responses.push_back("OK SHIP PURCHASED instance=" + instance + " hull=" + std::string(hull->id) +
                  " cost=" + std::to_string(hull->purchasePrice));
                responses.push_back("TRANSACTION SHIP_PURCHASE instance=" + instance + " hull=" +
                  std::string(hull->id) + " station=" + std::to_string(station) +
                  " credits=" + std::to_string(hull->purchasePrice));
              } catch (const std::exception& error) {
                profile = before; flightStateDirty = dirtyBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            appendShipyardState(responses, profile);
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
            appendShipyardState(responses, profile);
            appendFlightState(responses, profile);
          }
        }
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::refuel ||
                 request.command == helion::protocol::Command::outfit) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          if (request.command == helion::protocol::Command::refuel) {
            if (!activeShip(profile).flight.docked) responses.push_back("ERR dock-required");
            else {
              const auto before = profile;
              const bool dirtyBefore = flightStateDirty;
              helion::flight::FuelTransaction transaction;
              const auto result = helion::flight::refuel(activeShip(profile).flight, profile.credits, &transaction);
              if (result.rfind("OK", 0) == 0) {
                try { persistStateLocked(); }
                catch (const std::exception& error) {
                  profile = before;
                  flightStateDirty = dirtyBefore;
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
            responses.push_back(loadoutLine(profile));
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
              if (ownsModule(profile, request.second)) responses.push_back("ERR module-owned");
              else if (profile.credits < module->price) responses.push_back("ERR insufficient-credits");
              else {
                const auto before = profile;
                const bool dirtyBefore = flightStateDirty;
                profile.credits -= module->price;
                activeShip(profile).ownedModules.push_back(module->id);
                try {
                  persistStateLocked();
                  responses.push_back("OK MODULE BOUGHT module=" + std::string(module->id) +
                                     " slot=" + helion::loadout::slotName(module->slot) +
                                     " cost=" + std::to_string(module->price));
                  responses.push_back("TRANSACTION MODULE_PURCHASE module=" + std::string(module->id) +
                                     " slot=" + helion::loadout::slotName(module->slot) +
                                     " credits=" + std::to_string(module->price));
                } catch (const std::exception& error) {
                  profile = before;
                  flightStateDirty = dirtyBefore;
                  responses.push_back("ERR persistence-failed");
                  std::cerr << error.what() << '\n';
                }
              }
            } else if (request.first == "FIT") {
              if (!ownsModule(profile, request.second)) responses.push_back("ERR module-not-owned");
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
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::upgrade || request.command == helion::protocol::Command::repair) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          auto& owned = activeShip(profile);
          if (!owned.flight.docked) responses.push_back("ERR dock-required");
          else if (request.command == helion::protocol::Command::repair) {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            const auto* defense = fittedModule(profile, helion::loadout::Slot::defense);
            const auto result = helion::flight::repairToCapacity(owned.flight, profile.credits,
              helion::ships::hullCapacity(owned, defense ? defense->hullBonus : 0));
            if (result.rfind("OK", 0) == 0) {
              try { persistStateLocked(); }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
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
              profile.credits -= cost;
              ++level;
              refreshDerivedShipState(profile);
              try {
                persistStateLocked();
                responses.push_back("OK UPGRADE " + request.first + " level=" + std::to_string(level) + " cost=" + std::to_string(cost));
              } catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            appendFlightState(responses, profile);
          }
        }
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::contacts) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          for (const auto& contact : contactsFor(user)) responses.push_back(helion::flight::contactLine(contact));
          responses.push_back("CONTACTS END");
        }
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::buy || request.command == helion::protocol::Command::sell) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          int quantity = 0;
          try { quantity = std::stoi(request.second); }
          catch (...) { responses.push_back("ERR invalid-quantity"); }
          if (responses.empty()) {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            auto& owned = activeShip(profile);
            const auto result = helion::flight::trade(owned.flight, profile.credits,
              request.command == helion::protocol::Command::buy, request.first, quantity,
              helion::ships::cargoCapacity(owned));
            if (result.rfind("OK", 0) == 0) {
              if (request.command == helion::protocol::Command::buy)
                helion::career::purchased(profile.career, owned.flight.station, request.first, quantity);
              try { persistStateLocked(); }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            if (responses.empty()) responses.push_back(result);
            appendFlightState(responses, profile);
          }
        }
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::launch ||
                 request.command == helion::protocol::Command::input ||
                 request.command == helion::protocol::Command::flight ||
                 request.command == helion::protocol::Command::mine ||
                 request.command == helion::protocol::Command::dock) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
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
              result = helion::flight::dock(flight, profile.credits, profile.experience,
                                            docking ? &transaction : nullptr);
            }
            if (result.rfind("OK", 0) == 0) {
              if (request.command == helion::protocol::Command::launch) {
                resetCombatAfterLaunch(profile);
                addGalNetEvent("red-wake-kepler-activity", "Kepler Authority warns of Red Wake activity in the mining sector.");
              }
              if (request.command == helion::protocol::Command::mine && profile.missionStage == 1)
                profile.missionOreMined = true;
              try { persistStateLocked(); }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                galnetEvents = galnetBefore;
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
        for (const auto& response : responses) sendLine(fd, response);
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
  std::signal(SIGPIPE, SIG_IGN);
  std::signal(SIGINT, stopSignal);
  std::signal(SIGTERM, stopSignal);
  int port = kDefaultPort;
  std::size_t maxClients = kDefaultMaxClients;
  std::string bindAddress = "127.0.0.1";
  std::string certificate, privateKey;
  int positional = 0;
  if (!helion::ships::validateRegistry()) {
    std::cerr << "invalid canonical ship registry\n";
    return 1;
  }
  auto number = [](const std::string& value, unsigned long maximum) -> unsigned long {
    std::size_t end = 0;
    const unsigned long parsed = std::stoul(value, &end);
    if (end != value.size() || parsed == 0 || parsed > maximum) throw std::invalid_argument("invalid number");
    return parsed;
  };
  try {
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--bind" && i + 1 < argc) bindAddress = argv[++i];
      else if (arg == "--cert" && i + 1 < argc) certificate = argv[++i];
      else if (arg == "--key" && i + 1 < argc) privateKey = argv[++i];
      else if (arg == "--max-clients" && i + 1 < argc) maxClients = number(argv[++i], 1024);
      else if (arg.rfind("--", 0) == 0) throw std::invalid_argument("unknown option");
      else if (positional++ == 0) port = static_cast<int>(number(arg, 65535));
      else if (positional == 2) dataPath = arg;
      else throw std::invalid_argument("too many arguments");
    }
  } catch (const std::exception&) {
    std::cerr << "usage: helion_server [port] [data-file] [--bind IPv4-address] [--max-clients 1..1024] --cert certificate.pem --key private-key.pem\n";
    return 2;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, bindAddress.c_str(), &address.sin_addr) != 1) {
    std::cerr << "--bind requires a numeric IPv4 address\n";
    return 2;
  }
  helion::tls::Context tlsContext(nullptr, SSL_CTX_free);
  try { tlsContext = helion::tls::serverContext(certificate, privateKey); }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
  std::unique_ptr<helion::server::DataLock> dataLock;
  try {
    dataLock = std::make_unique<helion::server::DataLock>(dataPath);
    std::lock_guard<std::mutex> lock(stateMutex);
    dummyPasswordHash = helion::security::hashPassword(std::string(12, 'x'));
    loadState();
    migrateLegacyProfiles();
  } catch (const std::exception& error) {
    std::cerr << "unable to load or securely migrate persistence: " << error.what() << '\n';
    return 1;
  }
  const int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) { std::perror("socket"); return 1; }
  int yes = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
      listen(server, static_cast<int>(maxClients)) < 0) {
    std::perror("bind/listen");
    close(server);
    return 1;
  }
  std::cout << "Helion server listening on " << bindAddress << ':' << port
            << " TLS enabled; max-clients=" << maxClients << '\n';
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
  double accumulator = 0;
  while (!stopping) {
    const auto now = std::chrono::steady_clock::now();
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
    reap();
    pollfd ready{server, POLLIN, 0};
    const int polled = poll(&ready, 1, 16);
    if (polled < 0 && errno == EINTR) continue;
    if (polled < 0) { std::perror("poll"); break; }
    if (polled == 0 || !(ready.revents & POLLIN)) continue;
    sockaddr_in peer{};
    socklen_t length = sizeof(peer);
    const int client = accept(server, reinterpret_cast<sockaddr*>(&peer), &length);
    if (client < 0) {
      if (errno == EINTR) continue;
      std::perror("accept");
      break;
    }
    timeval readTimeout{kReadTimeoutSeconds, 0};
    timeval writeTimeout{kWriteTimeoutSeconds, 0};
    if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &readTimeout, sizeof(readTimeout)) != 0 ||
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &writeTimeout, sizeof(writeTimeout)) != 0) {
      close(client);
      continue;
    }
    if (!limit.tryAcquire()) {
      // Refuse overload before a handshake; never emit plaintext on the TLS port.
      close(client);
      continue;
    }
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
  close(server);
  {
    std::lock_guard<std::mutex> lock(transportMutex);
    for (const auto& entry : transports) shutdown(entry.first, SHUT_RDWR);
  }
  for (auto& worker : workers) worker.thread.join();
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (flightStateDirty) {
      try { persistStateLocked(); }
      catch (const std::exception& error) { std::cerr << "shutdown flight checkpoint failed: " << error.what() << '\n'; }
    }
  }
  return 0;
}
