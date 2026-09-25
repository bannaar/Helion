#pragma once

#include "shared/flight.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace helion::economy {

enum class PricingPolicy { productionLike, testConvenience };
enum class CurrencyFlow { faucet, sink, transfer };

struct MarketDefinition {
  int station = 0;
  std::string_view stationId;
  std::string_view systemId;
  std::string_view regionId;
  std::string_view ownerId;
  std::string_view commodityId;
  bool npcSells = false;
  bool npcBuys = false;
  int canonicalSellPrice = 0; // NPC sells to the player.
  int canonicalBuyPrice = 0;  // NPC buys from the player.
  int targetStock = 0;
  int stockRestock = 0;
  int targetDemand = 0;
  int demandRestock = 0;
  std::int64_t targetBuyBudget = 0;
  std::int64_t budgetRestock = 0;
};

struct OrderState {
  int station = 0;
  std::string commodityId;
  int stock = 0;
  int demand = 0;
  std::int64_t buyBudget = 0;
};

inline constexpr std::size_t kMarketCount = 6;
using MarketState = std::array<OrderState, kMarketCount>;

struct RestockResult {
  std::int64_t unitsAdded = 0;
  std::int64_t demandAdded = 0;
  std::int64_t budgetAdded = 0;
};

struct TradeResult {
  std::string line;
  bool committed = false;
  bool playerBuying = false;
  int station = 0;
  std::string commodityId;
  int quantity = 0;
  int unitPrice = 0;
  int total = 0;
};

struct DockSaleResult {
  std::string line;
  bool committed = false;
  flight::DockTransaction transaction;
};

struct Telemetry {
  std::int64_t creditsCreated = 0;
  std::int64_t creditsDestroyed = 0;
  std::int64_t creditsTransferred = 0;
  std::int64_t starterGrants = 0;
  std::int64_t missionPayouts = 0;
  std::int64_t combatPayouts = 0;
  std::int64_t npcBuyVolume = 0;
  std::int64_t npcBuyValue = 0;
  std::int64_t npcSellVolume = 0;
  std::int64_t npcSellValue = 0;
  std::int64_t marketTransactions = 0;
  std::int64_t shipPurchaseValue = 0;
  std::int64_t modulePurchaseValue = 0;
  std::int64_t serviceValue = 0;
  std::int64_t feesPaid = 0;
  std::int64_t taxesPaid = 0;
  std::int64_t oreMined = 0;
  std::int64_t restockedUnits = 0;
  std::int64_t restockedDemand = 0;
  std::int64_t restockedNpcBudget = 0;
};

const std::array<MarketDefinition, kMarketCount>& marketDefinitions();
const MarketDefinition* findDefinition(int station, std::string_view commodityId);
OrderState* findOrder(MarketState& state, int station, std::string_view commodityId);
const OrderState* findOrder(const MarketState& state, int station, std::string_view commodityId);
MarketState seedMarkets(PricingPolicy policy);
bool validateMarkets(const MarketState& state, PricingPolicy policy);
int npcSellPrice(const MarketDefinition& definition, PricingPolicy policy);
int npcBuyPrice(const MarketDefinition& definition, PricingPolicy policy);
RestockResult restock(MarketState& state, PricingPolicy policy);
TradeResult trade(MarketState& markets, flight::State& ship, int& credits, bool playerBuying,
                  std::string_view commodityId, int quantity, int cargoCapacity,
                  PricingPolicy policy);
DockSaleResult dockAndSellOre(MarketState& markets, flight::State& ship, int& credits,
                              int& experience, PricingPolicy policy);

void recordCurrency(Telemetry& telemetry, CurrencyFlow flow, std::int64_t credits);
void recordMarketTrade(Telemetry& telemetry, const TradeResult& transaction);
void recordOreSale(Telemetry& telemetry, const flight::DockTransaction& transaction);
void recordRestock(Telemetry& telemetry, const RestockResult& result);
bool validateTelemetry(const Telemetry& telemetry);
std::string serializeTelemetry(const Telemetry& telemetry);
std::optional<Telemetry> parseTelemetry(std::string_view value);
std::string telemetryLine(const Telemetry& telemetry, std::int64_t creditsInCirculation);
std::string marketLine(const MarketDefinition& definition, const OrderState& state,
                       PricingPolicy policy);

} // namespace helion::economy
