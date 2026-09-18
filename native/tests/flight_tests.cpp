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
  State damaged;
  launch(damaged); damaged.x=0; damaged.y=320; damaged.vy=-200; damaged.cargo=2;
  step(damaged,1.0/60);
  check(damaged.hull < damaged.maxHull && damaged.hull > 0 && !damaged.docked && damaged.cargo==2,
    "asteroid impact damages hull without destroying ship");
  damaged.hull=0; damaged.docked=true; damaged.cargo=2; int repairCredits=1000;
  check(launch(damaged)=="ERR repair-required", "disabled ship requires repair");
  check(repair(damaged,repairCredits,2)=="OK REPAIRED hull=125 cost=375" &&
    damaged.hull==125 && repairCredits==625, "station repair restores upgraded hull");
  Contact contact{"HAULER-7", "hauler", 12, -3, 1, false};
  Contact restoredContact;
  check(readContact(contactLine(contact), restoredContact) && restoredContact.id == "HAULER-7" &&
        restoredContact.kind == "hauler", "contact roundtrip");
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
