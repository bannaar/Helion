#include "shared/flight.h"
#include "shared/protocol.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void check(bool condition,const char* name) {
  if(!condition) { std::cerr<<"FAILED: "<<name<<'\n'; std::exit(1); }
}
void run(helion::flight::State& state,int frames,helion::flight::Input input) {
  for(int i=0;i<frames;++i) {
    state.input=input; state.inputAge=0;
    helion::flight::step(state,1.0/60);
  }
}
}
int main() {
  using namespace helion::flight;
  State left,right;
  check(launch(left)=="OK LAUNCHED","launch"); right=left;
  run(left,20,controls(true,true,false,false,true)); run(right,20,controls(true,false,true,false,true));
  const auto unfocused=controls(true,true,false,false,false);
  check(unfocused.thrust==0 && unfocused.turn==0 && unfocused.brake==1,"focus loss clears actions");
  check(left.x<0 && left.y>100 && left.yaw>0,"A turns nose and travel left while thrusting");
  check(right.x>0 && right.y>100 && right.yaw<0,"D turns nose and travel right while thrusting");
  const double moving=speed(left); run(left,30,{0,0,1});
  check(speed(left)<moving*0.2,"S brakes");
  run(right,30,{1,0,0});
  for(int i=0;i<180;++i) step(right,1.0/60);
  check(speed(right)<0.1,"stale input engages brake");
  State levelOne, levelFive;
  launch(levelOne); launch(levelFive);
  levelOne.input = {1, 0, 0}; levelOne.inputAge = 0;
  levelFive.input = {1, 0, 0}; levelFive.inputAge = 0;
  step(levelOne, 1.0 / 60, 1);
  step(levelFive, 1.0 / 60, 5);
  check(speed(levelOne) > 0 && speed(levelFive) > speed(levelOne) &&
    speed(levelFive) < speed(levelOne) * 1.6, "engine upgrade bounds thrust acceleration");
  State braking = levelFive;
  braking.vy = 120;
  braking.input = {0, 0, 1}; braking.inputAge = 0;
  const double beforeBrake = speed(braking);
  step(braking, 1.0 / 60, 5);
  check(speed(braking) < beforeBrake, "engine upgrade preserves braking");
  State staleAssist = levelFive;
  staleAssist.vy = 120;
  staleAssist.input = {1, 0, 0}; staleAssist.inputAge = 0.6;
  const double beforeAssist = speed(staleAssist);
  step(staleAssist, 1.0 / 60, 5);
  check(speed(staleAssist) < beforeAssist, "engine upgrade preserves stale input assist");
  State explicitLevelOne = levelOne;
  State defaultLevelOne = levelOne;
  explicitLevelOne = State{}; defaultLevelOne = State{};
  launch(explicitLevelOne); launch(defaultLevelOne);
  explicitLevelOne.input = {1, 0, 0}; defaultLevelOne.input = {1, 0, 0};
  explicitLevelOne.inputAge = 0; defaultLevelOne.inputAge = 0;
  step(explicitLevelOne, 1.0 / 60, 1);
  step(defaultLevelOne, 1.0 / 60);
  check(std::abs(speed(explicitLevelOne) - speed(defaultLevelOne)) < 1e-9, "level one remains default physics");
  State fuelShip;
  launch(fuelShip);
  const double startingFuel = fuelShip.fuel;
  fuelShip.input = {1, 0, 0}; fuelShip.inputAge = 0;
  run(fuelShip,60,fuelShip.input);
  check(fuelShip.fuel < startingFuel && fuelShip.fuel > 0,"thrust consumes bounded fuel");
  State standardDrive, efficientDrive;
  launch(standardDrive); launch(efficientDrive);
  standardDrive.input = {1, 0, 0}; efficientDrive.input = {1, 0, 0};
  standardDrive.inputAge = 0; efficientDrive.inputAge = 0;
  for (int i = 0; i < 60; ++i) {
    step(standardDrive,1.0 / 60,1,1.0);
    step(efficientDrive,1.0 / 60,1,0.65);
  }
  check(efficientDrive.fuel > standardDrive.fuel,"efficient engine reduces fuel consumption");
  State emptyFuel;
  emptyFuel.fuel = 0;
  check(launch(emptyFuel) == "ERR insufficient-fuel","empty fuel prevents launch");
  emptyFuel.docked = false; emptyFuel.input = {1, 0, 0}; emptyFuel.inputAge = 0;
  step(emptyFuel,1.0 / 60);
  check(emptyFuel.fuel == 0 && speed(emptyFuel) == 0,"empty fuel prevents thrust");
  State refuelShip;
  refuelShip.fuel = 45;
  int refuelCredits = 500;
  FuelTransaction refuelTransaction;
  check(refuel(refuelShip,refuelCredits,&refuelTransaction) == "OK REFUELED amount=55 cost=110" &&
        refuelShip.fuel == refuelShip.maxFuel && refuelCredits == 390 &&
        refuelTransaction.fuelAdded == 55 && refuelTransaction.creditsSpent == 110,
        "station refueling charges for authoritative quantity");
  FuelTransaction restoredFuelTransaction;
  check(readFuelTransaction(fuelTransactionLine(refuelTransaction),restoredFuelTransaction) &&
        restoredFuelTransaction.creditsSpent == 110 && restoredFuelTransaction.fuelAfter == 100,
        "refuel transaction protocol roundtrip");
  refuelShip.fuel = 0; refuelCredits = 0;
  check(refuel(refuelShip,refuelCredits) == "ERR insufficient-credits" && refuelShip.fuel == 0 && refuelCredits == 0,
        "insufficient credits cannot create fuel");
  State unequipped;
  launch(unequipped); unequipped.y = 210;
  check(mine(unequipped,false) == "ERR mining-module-required" && unequipped.cargo == 0,
        "mining requires fitted equipment");
  check(mine(unequipped,true,0.65) == "OK MINED cargo=1" && unequipped.cooldown > 0.8 && unequipped.cooldown < 0.9,
        "mining module changes extractor cooldown");
  State ship;
  check(mine(ship)=="ERR launch-required","cannot mine docked");
  launch(ship);
  check(mine(ship)=="ERR asteroid-out-of-range","cannot mine remotely");
  ship.y=210; ship.vy=80;
  check(mine(ship)=="ERR slow-down","cannot mine at speed");
  ship.vy=0;
  check(mine(ship)=="OK MINED cargo=1","extract ore");
  check(mine(ship)=="ERR mining-cooldown" && ship.cargo==1,"no command-spam mining");
  for(int i=1;i<kCargoCapacity;++i) { run(ship,76,{0,0,1}); check(mine(ship).rfind("OK",0)==0,"refill extractor"); }
  check(mine(ship)=="ERR cargo-full","bounded cargo");
  int credits=1500,xp=0;
  check(dock(ship,credits,xp)=="ERR station-out-of-range" && credits==1500,"no remote selling");
  ship.x=0;ship.y=50;
  check(dock(ship,credits,xp)=="OK DOCKED earned=480","sell full cargo");
  check(credits==1980 && xp==40 && ship.cargo==0 && ship.docked,"reward and clear cargo");
  State transactionShip;
  launch(transactionShip); transactionShip.x=0; transactionShip.y=50; transactionShip.cargo=2;
  int transactionCredits=1500, transactionXp=0;
  DockTransaction transaction;
  check(dock(transactionShip,transactionCredits,transactionXp,&transaction)=="OK DOCKED earned=120" &&
        transaction.station==0 && transaction.cargoSold==2 && transaction.unitPrice==60 &&
        transaction.creditsEarned==120 && transaction.experienceEarned==10,
        "structured dock sale result");
  DockTransaction restoredTransaction;
  check(readDockTransaction(dockTransactionLine(transaction),restoredTransaction) &&
        restoredTransaction.station==transaction.station &&
        restoredTransaction.cargoSold==transaction.cargoSold &&
        restoredTransaction.creditsEarned==transaction.creditsEarned &&
        restoredTransaction.experienceEarned==transaction.experienceEarned,
        "dock sale protocol roundtrip");
  check(!readDockTransaction("TRANSACTION DOCK_SALE station=0 quantity=2 unit-price=60 credits=999 experience=10",
                            restoredTransaction), "reject forged dock sale result");
  check(trade(ship, credits, true, "food", 3) == "OK BOUGHT food quantity=3 total=60" &&
        ship.food == 3 && credits == 1920, "buy station supplies");
  check(trade(ship, credits, true, "parts", 2) == "OK BOUGHT parts quantity=2 total=130" &&
        ship.parts == 2 && cargoUsed(ship) == 5, "buy ship parts");
  check(trade(ship, credits, false, "food", 1) == "OK SOLD food quantity=1 total=16" &&
        ship.food == 2 && credits == 1806, "sell station supplies");
  check(trade(ship, credits, true, "food", 8) == "ERR cargo-full", "market enforces hold capacity");
  ship.docked = false;
  check(trade(ship, credits, true, "food", 1) == "ERR dock-required", "market requires docking");
  ship.docked = true;
  check(dock(ship,credits,xp)=="ERR already-docked" && credits==1806,"no duplicate reward");
  State restored;
  int restoredCredits=0,restoredXp=0;
  check(readSnapshot(snapshot(ship,credits,xp),restored,restoredCredits,restoredXp) &&
    restored.docked && restoredCredits==1806 && restoredXp==40,"snapshot roundtrip");
  check(!readSnapshot("FLIGHT nan 0 0 0 0 1 0 0 0 0",restored,restoredCredits,restoredXp),"reject nonfinite state");
  check(!readSnapshot("FLIGHT 0 0 0 0 0 1 999 0 0 0",restored,restoredCredits,restoredXp),"reject invalid cargo");
  State restoredFuel;
  check(readFuelLine("FUEL 42.50 100.00",restoredFuel) && restoredFuel.fuel == 42.5 && restoredFuel.maxFuel == 100,
        "fuel protocol roundtrip");
  check(!readFuelLine("FUEL -1 100",restoredFuel),"reject invalid fuel protocol");
  State statusState;
  statusState.destroyed = true; statusState.weaponCooldown = 0.75;
  State restoredStatus;
  check(readCombatStatus(combatStatusLine(statusState), restoredStatus) && restoredStatus.destroyed &&
        std::abs(restoredStatus.weaponCooldown - 0.75) < 0.001, "combat status protocol roundtrip");
  check(!readCombatStatus("COMBAT STATUS destroyed=2 weapon-cooldown=0", restoredStatus), "reject invalid combat status");
  State destroyedShip;
  launch(destroyedShip);
  destroyedShip.destroyed = true;
  destroyedShip.docked = false;
  destroyedShip.hull = 0;
  destroyedShip.cargo = 3;
  destroyedShip.fuel = 4;
  const double destroyedX = destroyedShip.x;
  step(destroyedShip, 1.0 / 60);
  check(destroyedShip.x == destroyedX && mine(destroyedShip) == "ERR recovery-required",
        "destroyed ship stops ordinary flight and mining");
  check(recover(destroyedShip).rfind("OK RECOVERED station=0 hull=50 fuel=100", 0) == 0 &&
        destroyedShip.docked && !destroyedShip.destroyed && destroyedShip.cargo == 0 &&
        destroyedShip.hull == 50 && destroyedShip.fuel == destroyedShip.maxFuel,
        "recovery restores a playable safe state without cargo");
  check(recover(destroyedShip) == "ERR recovery-not-required", "recovery cannot be replayed");
  State damaged;
  launch(damaged); damaged.x=0; damaged.y=320; damaged.vy=-200; damaged.cargo=2;
  step(damaged,1.0/60);
  check(damaged.hull < damaged.maxHull && damaged.hull > 0 && !damaged.docked && damaged.cargo==2,
    "asteroid impact damages hull without destroying ship");
  State impactDestruction;
  launch(impactDestruction); impactDestruction.x=0; impactDestruction.y=320;
  impactDestruction.vy=-200; impactDestruction.hull=1; impactDestruction.cargo=2;
  step(impactDestruction,1.0/60);
  check(impactDestruction.destroyed && impactDestruction.hull==0 && !impactDestruction.docked &&
        impactDestruction.cargo==0, "impact destruction is bounded and loses cargo");
  damaged.hull=0; damaged.docked=true; damaged.cargo=2; int repairCredits=1000;
  damaged.destroyed = true;
  check(launch(damaged)=="ERR recovery-required", "destroyed ship requires recovery");
  damaged.destroyed = false;
  check(launch(damaged)=="ERR repair-required", "disabled ship requires repair");
  check(repair(damaged,repairCredits,2)=="OK REPAIRED hull=125 cost=375" &&
    damaged.hull==125 && repairCredits==625, "station repair restores upgraded hull");
  Contact contact{"HAULER-7", "hauler", 12, -3, 1, false};
  Contact restoredContact;
  check(readContact(contactLine(contact), restoredContact) && restoredContact.id == "HAULER-7" &&
        restoredContact.kind == "hauler", "contact roundtrip");
  Contact hostile{"RAIDER-7", "hostile", 0, 420, 0, false, true, 75, 100};
  check(readContact(contactLine(hostile), restoredContact) && restoredContact.hostile &&
        restoredContact.hull == 75 && restoredContact.maxHull == 100, "hostile contact roundtrip");
  launch(ship); ship.y=280; step(ship,1.0/60);
  check(std::hypot(ship.x,ship.y-280)>=42,"asteroid collision resolves even at center");
  ship.x=1400; step(ship,1.0/60);
  check(std::hypot(ship.x,ship.y)<=1200.001,"sector boundary");
  for(const auto* bad:{"INPUT 2 0 0","INPUT 1 nan 0","INPUT 1 0 -1","INPUT 1 0 0 extra","LAUNCH other","DOCK 50000"})
    check(helion::protocol::parseRequest(bad).command==helion::protocol::Command::invalid,"reject forged input");
  const auto input=helion::protocol::parseRequest("INPUT 1 -1 0");
  check(input.command==helion::protocol::Command::input && input.turn==-1,"valid input");
  std::cout<<"flight gameplay and controls tests passed\n";
}
