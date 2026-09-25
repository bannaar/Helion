#include "shared/economy.h"
#include "shared/flight.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void check(bool condition, const char* label) {
  if (!condition) { std::cerr << "FAILED: " << label << '\n'; std::exit(1); }
}
}

int main() {
  using namespace helion;
  using economy::PricingPolicy;

  auto production = economy::seedMarkets(PricingPolicy::productionLike);
  auto test = economy::seedMarkets(PricingPolicy::testConvenience);
  check(economy::validateMarkets(production, PricingPolicy::productionLike) &&
        economy::validateMarkets(test, PricingPolicy::testConvenience),
        "production and TEST seeded markets validate");

  const auto* keplerFood = economy::findDefinition(0, "food");
  const auto* cinderFood = economy::findDefinition(1, "food");
  const auto* keplerOre = economy::findDefinition(0, "ore");
  check(keplerFood && cinderFood && keplerOre && keplerFood->ownerId == "authority.kepler" &&
        keplerOre->ownerId == "corp.orion" && keplerFood->regionId == "KEPLER_REACH",
        "markets have station system region and owner identity");
  check(economy::npcSellPrice(*keplerFood, PricingPolicy::productionLike) == 20 &&
        economy::npcSellPrice(*cinderFood, PricingPolicy::productionLike) == 40 &&
        economy::npcBuyPrice(*keplerOre, PricingPolicy::productionLike) == 60,
        "production-like prices preserve existing regional balance");
  check(economy::npcSellPrice(*keplerFood, PricingPolicy::testConvenience) == 100 &&
        economy::npcSellPrice(*cinderFood, PricingPolicy::testConvenience) == 100 &&
        economy::npcBuyPrice(*keplerFood, PricingPolicy::testConvenience) == 80,
        "TEST purchase price is 100 without buy-sell inversion");
  check(economy::npcSellPrice(*keplerFood, PricingPolicy::productionLike) !=
        economy::npcSellPrice(*keplerFood, PricingPolicy::testConvenience),
        "TEST convenience pricing cannot redefine production pricing");

  flight::State ship;
  int credits = 1000;
  const auto* initialFood = economy::findOrder(production, 0, "food");
  const int stockBefore = initialFood ? initialFood->stock : 0;
  auto purchase = economy::trade(production, ship, credits, true, "food", 2, 8,
                                 PricingPolicy::productionLike);
  check(purchase.committed && purchase.total == 40 && credits == 960 && ship.food == 2 &&
        economy::findOrder(production, 0, "food")->stock == stockBefore - 2,
        "NPC seeded sell order commits cargo credits and finite stock together");
  const auto saleBudgetBefore = economy::findOrder(production, 0, "food")->buyBudget;
  auto sale = economy::trade(production, ship, credits, false, "food", 1, 8,
                             PricingPolicy::productionLike);
  check(sale.committed && sale.total == 16 && credits == 976 && ship.food == 1 &&
        economy::findOrder(production, 0, "food")->buyBudget == saleBudgetBefore - 16,
        "NPC seeded buy order is a bounded currency transfer");

  auto rejectedMarkets = production;
  auto rejectedShip = ship;
  int rejectedCredits = credits;
  economy::findOrder(rejectedMarkets, 0, "parts")->stock = 0;
  const auto unavailable = economy::trade(rejectedMarkets, rejectedShip, rejectedCredits, true,
                                           "parts", 1, 8, PricingPolicy::productionLike);
  check(!unavailable.committed && unavailable.line == "ERR market-stock-unavailable" &&
        rejectedCredits == credits && rejectedShip.parts == ship.parts &&
        economy::findOrder(rejectedMarkets, 0, "parts")->stock == 0,
        "unavailable stock rejects without partial player or market mutation");
  economy::findOrder(rejectedMarkets, 0, "food")->demand = 0;
  const auto noDemand = economy::trade(rejectedMarkets, rejectedShip, rejectedCredits, false,
                                        "food", 1, 8, PricingPolicy::productionLike);
  check(!noDemand.committed && noDemand.line == "ERR market-demand-unavailable" &&
        rejectedCredits == credits && rejectedShip.food == ship.food,
        "exhausted NPC demand rejects atomically");

  auto depleted = economy::seedMarkets(PricingPolicy::productionLike);
  auto* depletedParts = economy::findOrder(depleted, 1, "parts");
  depletedParts->stock = 0;
  depletedParts->demand = 0;
  depletedParts->buyBudget = 0;
  const auto replenished = economy::restock(depleted, PricingPolicy::productionLike);
  check(depletedParts->stock == 8 && depletedParts->demand == 6 && depletedParts->buyBudget == 192 &&
        replenished.unitsAdded >= 8 && replenished.demandAdded >= 6 && replenished.budgetAdded >= 192,
        "production-like replenishment is finite and deterministic");

  auto oreMarkets = economy::seedMarkets(PricingPolicy::productionLike);
  flight::State miner;
  miner.docked = false;
  miner.y = 30;
  miner.cargo = 2;
  int minerCredits = 100;
  int minerExperience = 0;
  const auto oreBudgetBefore = economy::findOrder(oreMarkets, 0, "ore")->buyBudget;
  const auto oreSale = economy::dockAndSellOre(oreMarkets, miner, minerCredits, minerExperience,
                                               PricingPolicy::productionLike);
  check(oreSale.committed && oreSale.transaction.creditsEarned == 120 && minerCredits == 220 &&
        minerExperience == 10 && miner.cargo == 0 && miner.docked &&
        economy::findOrder(oreMarkets, 0, "ore")->buyBudget == oreBudgetBefore - 120,
        "ore docking uses the regional bounded NPC buy order");

  economy::Telemetry telemetry;
  economy::recordMarketTrade(telemetry, purchase);
  economy::recordMarketTrade(telemetry, sale);
  economy::recordOreSale(telemetry, oreSale.transaction);
  economy::recordCurrency(telemetry, economy::CurrencyFlow::faucet, 250);
  telemetry.missionPayouts += 250;
  economy::recordCurrency(telemetry, economy::CurrencyFlow::sink, 100);
  economy::recordRestock(telemetry, replenished);
  const auto encoded = economy::serializeTelemetry(telemetry);
  const auto decoded = economy::parseTelemetry(encoded);
  check(decoded && decoded->creditsCreated == 250 && decoded->creditsDestroyed == 100 &&
        decoded->creditsTransferred == 176 && decoded->npcSellVolume == 2 &&
        decoded->npcBuyVolume == 3 && decoded->marketTransactions == 3 &&
        decoded->missionPayouts == 250 && decoded->restockedUnits == replenished.unitsAdded,
        "faucet sink transfer market and restock telemetry round-trip deterministically");
  const auto telemetryWire = economy::telemetryLine(*decoded, 42);
  check(telemetryWire.find("credits-in-circulation=42") != std::string::npos &&
        telemetryWire.find("mission-payouts=250") != std::string::npos &&
        telemetryWire.find("ship-purchase-value=0") != std::string::npos &&
        telemetryWire.find("fees-paid=0 taxes-paid=0") != std::string::npos &&
        telemetryWire.find("restocked-npc-budget=") != std::string::npos,
        "observable telemetry reports implemented source sink transfer and restock classes");
  check(!economy::parseTelemetry(encoded + ":1") && !economy::parseTelemetry("-1"),
        "malformed or negative telemetry is rejected");

  std::cout << "regional seeded market, pricing, transaction and telemetry tests passed\n";
}
