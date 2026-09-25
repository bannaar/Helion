#include "shared/economy.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace helion::economy {
namespace {
constexpr int kTestBuyPrice = 100;
constexpr int kTestSellPrice = 80;
constexpr int kTestStock = 100000;
constexpr int kTestDemand = 100000;
constexpr std::int64_t kTestBudget = 10000000;

const std::array<MarketDefinition, kMarketCount> kDefinitions{{
  {0, "KEPLER", "KEPLER", "KEPLER_REACH", "authority.kepler", "food", true, true,
   20, 16, 120, 8, 80, 5, 1280, 80},
  {0, "KEPLER", "KEPLER", "KEPLER_REACH", "authority.kepler", "parts", true, true,
   65, 52, 50, 4, 80, 4, 4160, 208},
  {0, "KEPLER", "KEPLER", "KEPLER_REACH", "corp.orion", "ore", false, true,
   0, 60, 0, 0, 300, 12, 18000, 720},
  {1, "CINDER", "KEPLER", "KEPLER_REACH", "corp.orion", "food", true, true,
   40, 32, 80, 5, 50, 3, 1600, 96},
  {1, "CINDER", "KEPLER", "KEPLER_REACH", "corp.orion", "parts", true, true,
   40, 32, 120, 8, 100, 6, 3200, 192},
  {1, "CINDER", "KEPLER", "KEPLER_REACH", "corp.orion", "ore", false, true,
   0, 75, 0, 0, 300, 12, 22500, 900}
}};

int targetStock(const MarketDefinition& definition, PricingPolicy policy) {
  return policy == PricingPolicy::testConvenience && definition.npcSells ? kTestStock : definition.targetStock;
}
int targetDemand(const MarketDefinition& definition, PricingPolicy policy) {
  return policy == PricingPolicy::testConvenience && definition.npcBuys ? kTestDemand : definition.targetDemand;
}
std::int64_t targetBudget(const MarketDefinition& definition, PricingPolicy policy) {
  return policy == PricingPolicy::testConvenience && definition.npcBuys ? kTestBudget : definition.targetBuyBudget;
}

bool checkedTotal(int price, int quantity, int& total) {
  if (price <= 0 || quantity <= 0 || quantity > std::numeric_limits<int>::max() / price) return false;
  total = price * quantity;
  return true;
}

template <typename T>
bool nonnegative(const T& telemetry) {
  return telemetry >= 0;
}
}

const std::array<MarketDefinition, kMarketCount>& marketDefinitions() { return kDefinitions; }

const MarketDefinition* findDefinition(int station, std::string_view commodityId) {
  for (const auto& definition : kDefinitions)
    if (definition.station == station && definition.commodityId == commodityId) return &definition;
  return nullptr;
}

OrderState* findOrder(MarketState& state, int station, std::string_view commodityId) {
  for (auto& order : state)
    if (order.station == station && order.commodityId == commodityId) return &order;
  return nullptr;
}

const OrderState* findOrder(const MarketState& state, int station, std::string_view commodityId) {
  for (const auto& order : state)
    if (order.station == station && order.commodityId == commodityId) return &order;
  return nullptr;
}

MarketState seedMarkets(PricingPolicy policy) {
  MarketState state{};
  for (std::size_t i = 0; i < kDefinitions.size(); ++i) {
    const auto& definition = kDefinitions[i];
    state[i] = {definition.station, std::string(definition.commodityId),
      targetStock(definition, policy), targetDemand(definition, policy), targetBudget(definition, policy)};
  }
  return state;
}

bool validateMarkets(const MarketState& state, PricingPolicy policy) {
  std::unordered_set<std::string> keys;
  for (const auto& order : state) {
    const auto* definition = findDefinition(order.station, order.commodityId);
    const std::string key = std::to_string(order.station) + ":" + order.commodityId;
    if (!definition || !keys.emplace(key).second || order.stock < 0 || order.demand < 0 ||
        order.buyBudget < 0 || order.stock > std::max(1000000, targetStock(*definition, policy) * 10) ||
        order.demand > std::max(1000000, targetDemand(*definition, policy) * 10) ||
        order.buyBudget > std::max<std::int64_t>(1000000000, targetBudget(*definition, policy) * 10)) return false;
  }
  return keys.size() == kDefinitions.size();
}

int npcSellPrice(const MarketDefinition& definition, PricingPolicy policy) {
  if (!definition.npcSells) return 0;
  return policy == PricingPolicy::testConvenience ? kTestBuyPrice : definition.canonicalSellPrice;
}

int npcBuyPrice(const MarketDefinition& definition, PricingPolicy policy) {
  if (!definition.npcBuys) return 0;
  return policy == PricingPolicy::testConvenience ? kTestSellPrice : definition.canonicalBuyPrice;
}

RestockResult restock(MarketState& state, PricingPolicy policy) {
  RestockResult result;
  for (auto& order : state) {
    const auto* definition = findDefinition(order.station, order.commodityId);
    if (!definition) continue;
    const int stockBefore = order.stock;
    const int demandBefore = order.demand;
    const std::int64_t budgetBefore = order.buyBudget;
    const int stockStep = policy == PricingPolicy::testConvenience ? kTestStock : definition->stockRestock;
    const int demandStep = policy == PricingPolicy::testConvenience ? kTestDemand : definition->demandRestock;
    const std::int64_t budgetStep = policy == PricingPolicy::testConvenience ? kTestBudget : definition->budgetRestock;
    order.stock = std::min(targetStock(*definition, policy), order.stock + stockStep);
    order.demand = std::min(targetDemand(*definition, policy), order.demand + demandStep);
    order.buyBudget = std::min(targetBudget(*definition, policy), order.buyBudget + budgetStep);
    result.unitsAdded += order.stock - stockBefore;
    result.demandAdded += order.demand - demandBefore;
    result.budgetAdded += order.buyBudget - budgetBefore;
  }
  return result;
}

TradeResult trade(MarketState& markets, flight::State& ship, int& credits, bool playerBuying,
                  std::string_view commodityId, int quantity, int cargoCapacity,
                  PricingPolicy policy) {
  TradeResult result;
  result.playerBuying = playerBuying;
  result.station = ship.station;
  result.commodityId = std::string(commodityId);
  result.quantity = quantity;
  if (!ship.docked) { result.line = "ERR dock-required"; return result; }
  const auto* definition = findDefinition(ship.station, commodityId);
  auto* order = findOrder(markets, ship.station, commodityId);
  if (!definition || !order || commodityId == "ore") { result.line = "ERR unknown-commodity"; return result; }
  result.unitPrice = playerBuying ? npcSellPrice(*definition, policy) : npcBuyPrice(*definition, policy);
  if (!checkedTotal(result.unitPrice, quantity, result.total)) { result.line = "ERR invalid-quantity"; return result; }
  if (playerBuying) {
    if (!definition->npcSells || order->stock < quantity) { result.line = "ERR market-stock-unavailable"; return result; }
  } else if (!definition->npcBuys || order->demand < quantity || order->buyBudget < result.total) {
    result.line = "ERR market-demand-unavailable";
    return result;
  }
  result.line = flight::tradeAtPrice(ship, credits, playerBuying, std::string(commodityId), quantity,
                                     cargoCapacity, result.unitPrice);
  if (result.line.rfind("OK", 0) != 0) return result;
  if (playerBuying) {
    order->stock -= quantity;
    order->buyBudget = std::min(targetBudget(*definition, policy), order->buyBudget + result.total);
  } else {
    order->stock += quantity;
    order->demand -= quantity;
    order->buyBudget -= result.total;
  }
  result.committed = true;
  return result;
}

DockSaleResult dockAndSellOre(MarketState& markets, flight::State& ship, int& credits,
                              int& experience, PricingPolicy policy) {
  DockSaleResult result;
  const int station = flight::nearestStation(ship);
  const auto* definition = findDefinition(station, "ore");
  auto* order = findOrder(markets, station, "ore");
  if (!definition || !order) { result.line = "ERR market-unavailable"; return result; }
  const int quantity = ship.cargo;
  const int price = npcBuyPrice(*definition, policy);
  int total = 0;
  if (quantity > 0 && (!checkedTotal(price, quantity, total) || order->demand < quantity || order->buyBudget < total)) {
    result.line = "ERR market-demand-unavailable";
    return result;
  }
  result.line = flight::dockAtPrice(ship, credits, experience, price, &result.transaction);
  if (result.line.rfind("OK", 0) != 0) return result;
  if (quantity > 0) {
    order->stock += quantity;
    order->demand -= quantity;
    order->buyBudget -= total;
  }
  result.committed = true;
  return result;
}

void recordCurrency(Telemetry& telemetry, CurrencyFlow flow, std::int64_t credits) {
  if (credits <= 0) return;
  if (flow == CurrencyFlow::faucet) telemetry.creditsCreated += credits;
  else if (flow == CurrencyFlow::sink) telemetry.creditsDestroyed += credits;
  else telemetry.creditsTransferred += credits;
}

void recordMarketTrade(Telemetry& telemetry, const TradeResult& transaction) {
  if (!transaction.committed || transaction.quantity <= 0 || transaction.total <= 0) return;
  ++telemetry.marketTransactions;
  recordCurrency(telemetry, CurrencyFlow::transfer, transaction.total);
  if (transaction.playerBuying) {
    telemetry.npcSellVolume += transaction.quantity;
    telemetry.npcSellValue += transaction.total;
  } else {
    telemetry.npcBuyVolume += transaction.quantity;
    telemetry.npcBuyValue += transaction.total;
  }
}

void recordOreSale(Telemetry& telemetry, const flight::DockTransaction& transaction) {
  if (transaction.cargoSold <= 0 || transaction.creditsEarned <= 0) return;
  ++telemetry.marketTransactions;
  telemetry.npcBuyVolume += transaction.cargoSold;
  telemetry.npcBuyValue += transaction.creditsEarned;
  recordCurrency(telemetry, CurrencyFlow::transfer, transaction.creditsEarned);
}

void recordRestock(Telemetry& telemetry, const RestockResult& result) {
  telemetry.restockedUnits += result.unitsAdded;
  telemetry.restockedDemand += result.demandAdded;
  telemetry.restockedNpcBudget += result.budgetAdded;
}

bool validateTelemetry(const Telemetry& t) {
  return nonnegative(t.creditsCreated) && nonnegative(t.creditsDestroyed) &&
    nonnegative(t.creditsTransferred) && nonnegative(t.starterGrants) &&
    nonnegative(t.missionPayouts) && nonnegative(t.combatPayouts) &&
    nonnegative(t.npcBuyVolume) && nonnegative(t.npcBuyValue) &&
    nonnegative(t.npcSellVolume) && nonnegative(t.npcSellValue) &&
    nonnegative(t.marketTransactions) && nonnegative(t.shipPurchaseValue) &&
    nonnegative(t.modulePurchaseValue) && nonnegative(t.serviceValue) &&
    nonnegative(t.feesPaid) && nonnegative(t.taxesPaid) && nonnegative(t.oreMined) &&
    nonnegative(t.restockedUnits) && nonnegative(t.restockedDemand) &&
    nonnegative(t.restockedNpcBudget);
}

std::string serializeTelemetry(const Telemetry& t) {
  std::ostringstream out;
  out << t.creditsCreated << ':' << t.creditsDestroyed << ':' << t.creditsTransferred << ':'
      << t.starterGrants << ':' << t.missionPayouts << ':' << t.combatPayouts << ':'
      << t.npcBuyVolume << ':' << t.npcBuyValue << ':' << t.npcSellVolume << ':'
      << t.npcSellValue << ':' << t.marketTransactions << ':' << t.shipPurchaseValue << ':'
      << t.modulePurchaseValue << ':' << t.serviceValue << ':' << t.feesPaid << ':' << t.taxesPaid << ':'
      << t.oreMined << ':' << t.restockedUnits << ':' << t.restockedDemand << ':' << t.restockedNpcBudget;
  return out.str();
}

std::optional<Telemetry> parseTelemetry(std::string_view value) {
  std::istringstream input{std::string(value)};
  Telemetry t;
  char separator = 0;
  std::int64_t* fields[]{&t.creditsCreated, &t.creditsDestroyed, &t.creditsTransferred,
    &t.starterGrants, &t.missionPayouts, &t.combatPayouts, &t.npcBuyVolume, &t.npcBuyValue,
    &t.npcSellVolume, &t.npcSellValue, &t.marketTransactions, &t.shipPurchaseValue,
    &t.modulePurchaseValue, &t.serviceValue, &t.feesPaid, &t.taxesPaid, &t.oreMined,
    &t.restockedUnits, &t.restockedDemand, &t.restockedNpcBudget};
  for (std::size_t i = 0; i < std::size(fields); ++i) {
    if (!(input >> *fields[i])) return std::nullopt;
    if (i + 1 < std::size(fields) && (!(input >> separator) || separator != ':')) return std::nullopt;
  }
  std::string extra;
  if (input >> extra || !validateTelemetry(t)) return std::nullopt;
  return t;
}

std::string telemetryLine(const Telemetry& t, std::int64_t creditsInCirculation) {
  return "ECONOMY TELEMETRY credits-created=" + std::to_string(t.creditsCreated) +
    " credits-destroyed=" + std::to_string(t.creditsDestroyed) +
    " credits-transferred=" + std::to_string(t.creditsTransferred) +
    " credits-in-circulation=" + std::to_string(creditsInCirculation) +
    " starter-grants=" + std::to_string(t.starterGrants) +
    " mission-payouts=" + std::to_string(t.missionPayouts) +
    " combat-payouts=" + std::to_string(t.combatPayouts) +
    " npc-buy-volume=" + std::to_string(t.npcBuyVolume) +
    " npc-buy-value=" + std::to_string(t.npcBuyValue) +
    " npc-sell-volume=" + std::to_string(t.npcSellVolume) +
    " npc-sell-value=" + std::to_string(t.npcSellValue) +
    " market-transactions=" + std::to_string(t.marketTransactions) +
    " ship-purchase-value=" + std::to_string(t.shipPurchaseValue) +
    " module-purchase-value=" + std::to_string(t.modulePurchaseValue) +
    " service-value=" + std::to_string(t.serviceValue) +
    " fees-paid=" + std::to_string(t.feesPaid) + " taxes-paid=" + std::to_string(t.taxesPaid) +
    " ore-mined=" + std::to_string(t.oreMined) +
    " restocked-units=" + std::to_string(t.restockedUnits) +
    " restocked-demand=" + std::to_string(t.restockedDemand) +
    " restocked-npc-budget=" + std::to_string(t.restockedNpcBudget);
}

std::string marketLine(const MarketDefinition& definition, const OrderState& state,
                       PricingPolicy policy) {
  return "MARKET station=" + std::string(definition.stationId) +
    " station-id=" + std::to_string(definition.station) + " system=" + std::string(definition.systemId) +
    " region=" + std::string(definition.regionId) + " owner=" + std::string(definition.ownerId) +
    " commodity=" + std::string(definition.commodityId) +
    " sell-price=" + std::to_string(npcSellPrice(definition, policy)) +
    " buy-price=" + std::to_string(npcBuyPrice(definition, policy)) +
    " stock=" + std::to_string(state.stock) + " demand=" + std::to_string(state.demand) +
    " npc-buy-budget=" + std::to_string(state.buyBudget);
}

} // namespace helion::economy
