#include "server/test_admin.h"
#include "shared/protocol.h"
#include "shared/shipyard.h"
#include <cstdlib>
#include <iostream>

void check(bool condition, const char* text) {
  if (!condition) { std::cerr << text << '\n'; std::exit(1); }
}
int main() {
  using namespace helion::server;
  EnvironmentPolicy policy;
  policy.accounts = {{"admin", AccountRole::dev_admin}, {"tester", AccountRole::invited_tester},
    {"normal", AccountRole::normal_player}};
  check(!authorizeTestAdmin({policy, "admin", true}).empty(), "implicit environment denies admin");
  policy.environment = RuntimeEnvironment::private_test;
  check(!policy.admits("admin") && !policy.cheapEconomy(), "implicit TEST fails closed");
  policy.explicitEnvironment = true;
  check(policy.admits("tester") && !policy.admits("stranger") && policy.admits("normal"), "explicit invitation policy");
  check(authorizeTestAdmin({policy, "admin", true}).empty(), "admin authorized in TEST");
  check(!authorizeTestAdmin({policy, "admin", false}).empty(), "companion/anonymous cannot admin");
  check(!authorizeTestAdmin({policy, "tester", true}).empty() &&
    !authorizeTestAdmin({policy, "normal", true}).empty(), "nonadmin roles denied");
  check(policy.bypassShipRequirements("tester") && policy.bypassShipRequirements("admin") &&
    !policy.bypassShipRequirements("normal"), "TEST ship privilege is gameplay-only");
  helion::shipyard::Offer gated;
  gated.reputationId = "faction.compact"; gated.minimumReputation = 25;
  check(!helion::shipyard::meetsGameplayRequirements(gated, {}) &&
    helion::shipyard::meetsGameplayRequirements(gated, {}, policy.bypassShipRequirements("tester")), "TEST bypasses actual requirement predicate");
  policy.convenienceEconomy = false;
  check(!policy.cheapEconomy() && policy.releaseChannel() == "TEST", "production-like TEST economy supported");
  policy.environment = RuntimeEnvironment::production;
  check(!authorizeTestAdmin({policy, "admin", true}).empty() && !policy.bypassShipRequirements("admin") &&
    !policy.cheapEconomy() && policy.releaseChannel() == "STABLE", "production never enables TEST powers");
  check(!helion::shipyard::meetsGameplayRequirements(gated, {}, policy.bypassShipRequirements("admin")), "production requirements unchanged");
  policy.environment = static_cast<RuntimeEnvironment>(99);
  check(!policy.admits("admin") && !authorizeTestAdmin({policy, "admin", true}).empty(), "invalid environment fails closed");
  for (const auto* payload : {" TEST 1 ADD pilot gsc 50", " TEST 2 GRANT pilot ship TITAN_MULE", " TEST 3 RESET universe markets seed"})
    check(parseAdminOperation(payload).has_value(), "valid typed operation");
  for (const auto* payload : {" TEST x ADD pilot gsc 50", " PRODUCTION 1 ADD pilot gsc 50", " TEST 1 SQL pilot gsc 50",
      " TEST 1 ADD pilot password secret", " TEST 1 ADD pilot gsc 50 role=DEV_ADMIN", " TEST 1 ADD ../pilot gsc 50",
      " TEST 1 ADD pilot gsc", " TEST 1 ADD pilot gsc 50 environment=test"})
    check(!parseAdminOperation(payload), "invalid command cannot smuggle role/env/path/schema");
  check(helion::protocol::parseRequest("TESTADMIN bad secret").command == helion::protocol::Command::test_admin,
    "malformed admin requests still reach audited authorization");
  check(digest("a") != digest("b") && digest("a").size() == 64, "stable bounded request fingerprint");
  std::cout << "TEST environment, roles, authorization, grammar and ship policy passed\n";
}
