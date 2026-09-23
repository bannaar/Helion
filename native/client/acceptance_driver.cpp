#include "client/acceptance_driver.h"

#include "client/cockpit.h"
#include "client/ui.h"
#include "shared/career.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace helion::client {
namespace {
constexpr const char* kUser = "acceptance_pilot";
constexpr const char* kPassword = "Kepler-Accept-2026";
constexpr double kStageTimeout = 45.0;
constexpr double kSoakStageTimeout = 60.0;

void pushKey(Uint32 type, SDL_Keycode key) {
  SDL_Event event{};
  event.type = type;
  event.key.type = type;
  event.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
  event.key.keysym.sym = key;
  event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
  SDL_PushEvent(&event);
}
}

AcceptanceDriver::AcceptanceDriver(Phase phase, std::string captureDirectory, double soakSeconds)
    : phase_(phase), captureDirectory_(std::move(captureDirectory)), soakSeconds_(soakSeconds) {
  if (!captureDirectory_.empty()) std::filesystem::create_directories(captureDirectory_);
}

void AcceptanceDriver::advance(int next, double now) {
  stage_ = next;
  stageStarted_ = now;
  nextAction_ = now + 0.18;
}

void AcceptanceDriver::fail(std::string reason) {
  if (failed_) return;
  failed_ = true;
  failure_ = std::move(reason);
  setFlightKeys(false, false, false, true);
  std::cerr << "ACCEPTANCE FAIL stage=" << stage_ << " reason=" << failure_ << '\n';
}

std::string AcceptanceDriver::consumeCapture() {
  std::string result = std::move(pendingCapture_);
  pendingCapture_.clear();
  return result;
}

bool AcceptanceDriver::capture(std::string name) {
  if (captured_.count(name)) return true;
  if (!pendingCapture_.empty()) return false;
  pendingCapture_ = captureDirectory_ + "/" + name + ".bmp";
  captured_.insert(std::move(name));
  return false;
}

void AcceptanceDriver::click(const UiState& ui, UiControlId id, int index, int width, int height) {
  const auto controls = buildUiControls(ui);
  const auto found = std::find_if(controls.begin(), controls.end(), [=](const UiControl& control) {
    return control.id == id && (index < 0 || control.index == index);
  });
  if (found == controls.end()) { fail("control-not-rendered:" + uiControlName(id)); return; }
  const auto layout = hudLayout(width, height);
  const int x = static_cast<int>(layout.originX + (found->rect.x + found->rect.width * 0.5f) * layout.scale);
  const int y = static_cast<int>(layout.originY + (found->rect.y + found->rect.height * 0.5f) * layout.scale);
  SDL_Event down{};
  down.type = SDL_MOUSEBUTTONDOWN; down.button.button = SDL_BUTTON_LEFT;
  down.button.state = SDL_PRESSED; down.button.x = x; down.button.y = y;
  SDL_Event up = down;
  up.type = SDL_MOUSEBUTTONUP; up.button.type = SDL_MOUSEBUTTONUP; up.button.state = SDL_RELEASED;
  SDL_PushEvent(&down);
  SDL_PushEvent(&up);
}

void AcceptanceDriver::type(std::string_view text) {
  for (std::size_t offset = 0; offset < text.size();) {
    SDL_Event event{};
    event.type = SDL_TEXTINPUT;
    const std::size_t count = std::min<std::size_t>(text.size() - offset, sizeof(event.text.text) - 1);
    std::copy_n(text.data() + offset, count, event.text.text);
    event.text.text[count] = '\0';
    SDL_PushEvent(&event);
    offset += count;
  }
}

void AcceptanceDriver::tap(SDL_Keycode key) {
  pushKey(SDL_KEYDOWN, key);
  pushKey(SDL_KEYUP, key);
}

void AcceptanceDriver::setFlightKeys(bool forward, bool left, bool right, bool brake) {
  const auto update = [](bool desired, bool& current, SDL_Keycode key) {
    if (desired == current) return;
    pushKey(desired ? SDL_KEYDOWN : SDL_KEYUP, key);
    current = desired;
  };
  update(forward, forward_, SDLK_w);
  update(left, left_, SDLK_a);
  update(right, right_, SDLK_d);
  update(brake, brake_, SDLK_s);
}

void AcceptanceDriver::navigate(const View& view, double x, double y) {
  const double dx = x - view.ship.x;
  const double dy = y - view.ship.y;
  const double distance = std::hypot(dx, dy);
  const double desired = std::atan2(-dx, dy);
  const double delta = std::remainder(desired - view.ship.yaw, 2.0 * flight::kPi);
  const bool brake = distance < 58.0 || std::abs(delta) > 0.75;
  const bool thrust = distance > 42.0 && std::abs(delta) < 0.34;
  setFlightKeys(thrust, delta > 0.08, delta < -0.08, brake);
}

void AcceptanceDriver::tick(const View& view, int width, int height, double now) {
  if (!enabled() || done_ || failed_) return;
  if (stageStarted_ == 0) stageStarted_ = now;
  if (now - stageStarted_ > (phase_ == Phase::soak ? kSoakStageTimeout : kStageTimeout)) {
    fail("stage-timeout:x=" + std::to_string(static_cast<int>(view.ship.x)) +
         ":y=" + std::to_string(static_cast<int>(view.ship.y)) +
         ":speed=" + std::to_string(static_cast<int>(flight::speed(view.ship))) +
         ":hull=" + std::to_string(view.ship.hull) +
         ":destroyed=" + std::to_string(view.ship.destroyed) +
         ":contacts=" + std::to_string(view.contacts.size()) +
         ":target=" + (view.targetId.empty() ? std::string("none") : view.targetId) +
         ":cooldown=" + std::to_string(view.ship.weaponCooldown) +
         ":yaw=" + std::to_string(view.ship.yaw) +
         ":screen=" + std::to_string(static_cast<int>(view.ui.screen)) +
         ":station=" + std::to_string(view.ship.station) +
         ":parts=" + std::to_string(view.ship.parts) +
         ":supply=" + std::to_string(view.ui.career.supply) +
         ":mission-selected=" + std::to_string(view.ui.missionSelected) +
         ":pending=" + std::to_string(view.ui.commandPending) +
         ":status=" + view.ui.statusMessage.substr(0, 80));
    return;
  }
  if (now < nextAction_) return;

  if (phase_ == Phase::soak) {
    if (soakStarted_ > 0 && now - soakStarted_ >= soakSeconds_) {
      setFlightKeys(false, false, false, true);
      done_ = true;
      std::cout << "ACCEPTANCE PASS phase=soak cycles=" << cycles_ << std::endl;
      return;
    }
    switch (stage_) {
      case 0:
        if (view.ui.screen != UiScreen::account || !view.commandReady) return;
        click(view.ui, UiControlId::accountName, -1, width, height); type(kUser);
        click(view.ui, UiControlId::accountPassword, -1, width, height); type(kPassword);
        click(view.ui, UiControlId::accountLogin, -1, width, height); advance(1, now); return;
      case 1:
        if (!view.authenticated || !view.ui.careerKnown) return;
        if (!view.ui.career.complete) { fail("soak-requires-completed-career"); return; }
        soakStarted_ = now; advance(100, now); return;
      case 100:
        if (view.ship.destroyed) { tap(SDLK_r); nextAction_ = now + 1.0; return; }
        if (view.ship.docked) { setFlightKeys(false, false, false, false); tap(SDLK_F1); advance(101, now); return; }
        if (view.ui.screen != UiScreen::flight) tap(SDLK_ESCAPE);
        navigate(view, 0, 0);
        if (std::hypot(view.ship.x, view.ship.y) < 80 && flight::speed(view.ship) < 30) {
          setFlightKeys(false, false, false, true); tap(SDLK_f); nextAction_ = now + 0.5;
        }
        return;
      case 101:
        if (view.ui.screen != UiScreen::station) return;
        click(view.ui, UiControlId::stationMission, -1, width, height); advance(102, now); return;
      case 102:
        if (view.ui.screen != UiScreen::mission) return;
        tap(SDLK_F4); advance(103, now); return;
      case 103:
        if (view.ui.screen != UiScreen::market) return;
        click(view.ui, UiControlId::marketParts, 1, width, height);
        tap(SDLK_F7); advance(104, now); return;
      case 104:
        if (view.ui.screen != UiScreen::galnet) return;
        { SDL_Event wheel{}; wheel.type = SDL_MOUSEWHEEL; wheel.wheel.y = -1; SDL_PushEvent(&wheel); }
        tap(SDLK_F2); advance(105, now); return;
      case 105:
        if (view.ui.screen != UiScreen::profile) return;
        tap(SDLK_F9); advance(106, now); return;
      case 106:
        if (view.ui.screen != UiScreen::help) return;
        tap(SDLK_F8); advance(107, now); return;
      case 107:
        if (view.ui.screen != UiScreen::graphics) return;
        tap(SDLK_F1); advance(108, now); return;
      case 108:
        if (view.ui.screen != UiScreen::station) return;
        click(view.ui, UiControlId::stationLaunch, -1, width, height); advance(109, now); return;
      case 109:
        if (view.ship.docked) {
          setFlightKeys(false, false, false, false);
          if (view.ui.screen != UiScreen::station) tap(SDLK_F1);
          else click(view.ui, UiControlId::stationLaunch, -1, width, height);
          nextAction_ = now + 0.75;
          return;
        }
        if (view.ship.destroyed) { advance(100, now); return; }
        if (view.ui.screen != UiScreen::flight) {
          tap(SDLK_ESCAPE);
          nextAction_ = now + 0.5;
          return;
        }
        navigate(view, 0, 205);
        if (std::hypot(view.ship.x, view.ship.y - 280) < flight::kMineRange &&
            flight::speed(view.ship) < 30 && view.ship.cargo < flight::kCargoCapacity) {
          setFlightKeys(false, false, false, true); tap(SDLK_e);
        }
        if (!view.targetId.empty() && view.ship.weaponCooldown < 0.05) tap(SDLK_SPACE);
        if (now - stageStarted_ >= 3.0) { ++cycles_; advance(100, now); }
        return;
    }
  }

  if (phase_ == Phase::reconnect) {
    switch (stage_) {
      case 0:
        if (view.ui.screen != UiScreen::account || !view.commandReady) return;
        click(view.ui, UiControlId::accountName, -1, width, height); type(kUser);
        click(view.ui, UiControlId::accountPassword, -1, width, height); type(kPassword);
        click(view.ui, UiControlId::accountLogin, -1, width, height); advance(1, now); return;
      case 1:
        if (!view.authenticated || !view.ui.careerKnown) return;
        if (!view.ui.career.complete || view.ui.missionStage != 2 || view.ui.career.supply != 2 || view.ui.career.response != 2) {
          fail("persistent-career-state-missing"); return;
        }
        baselineCredits_ = view.credits;
        tap(SDLK_F2); advance(2, now); return;
      case 2:
        if (view.ui.screen != UiScreen::profile) return;
        if (!capture("reconnected-profile")) return;
        tap(SDLK_F5); advance(3, now); return;
      case 3:
        if (view.ui.screen != UiScreen::mission) return;
        click(view.ui, UiControlId::missionRow, 0, width, height);
        // The completed contract has no enabled turn-in control. Clicking the
        // rendered disabled region must not emit a command or reclaim rewards.
        click(view.ui, UiControlId::missionTurnIn, -1, width, height);
        advance(4, now); return;
      case 4:
        if (view.credits != baselineCredits_) { fail("completed-reward-reclaimed"); return; }
        tap(SDLK_F1); advance(5, now); return;
      case 5:
        if (view.ui.screen != UiScreen::station) return;
        click(view.ui, UiControlId::stationLaunch, -1, width, height); advance(6, now); return;
      case 6:
        if (view.ship.docked) return;
        if (!capture("post-completion-free-play")) return;
        done_ = true;
        std::cout << "ACCEPTANCE PASS phase=reconnect credits=" << view.credits
                  << " xp=" << view.experience << " salvage=" << view.ui.salvage << '\n';
        return;
    }
  }

  const auto recoverForTravel = [&] {
    if (!view.ship.destroyed) return false;
    setFlightKeys(false, false, false, true);
    tap(SDLK_r);
    nextAction_ = now + 1.0;
    return true;
  };
  const auto repairAndRelaunch = [&] {
    setFlightKeys(false, false, false, false);
    if (view.ui.screen != UiScreen::station) {
      tap(SDLK_F1);
      nextAction_ = now + 0.5;
      return;
    }
    const int repairCost = (view.ship.maxHull - view.ship.hull) * 3;
    if (repairCost > 0 && view.credits >= repairCost) {
      click(view.ui, UiControlId::stationRepair, -1, width, height);
    } else {
      click(view.ui, UiControlId::stationLaunch, -1, width, height);
      stageStarted_ = now;
    }
    nextAction_ = now + 1.0;
  };

  switch (stage_) {
    case 0:
      if (view.ui.screen != UiScreen::account || !view.commandReady) return;
      click(view.ui, UiControlId::accountName, -1, width, height); type(kUser);
      click(view.ui, UiControlId::accountPassword, -1, width, height); type(kPassword);
      click(view.ui, UiControlId::accountDisplay, -1, width, height); type("Kepler Pilot");
      click(view.ui, UiControlId::accountCreate, -1, width, height); advance(1, now); return;
    case 1:
      if (!view.authenticated || view.ui.screen != UiScreen::help) return;
      if (!capture("onboarding")) return;
      click(view.ui, UiControlId::introDismiss, -1, width, height); advance(2, now); return;
    case 2:
      if (!view.ui.career.introDismissed || view.ui.screen != UiScreen::station) return;
      click(view.ui, UiControlId::stationMission, -1, width, height); advance(3, now); return;
    case 3:
      if (view.ui.screen != UiScreen::mission) return;
      click(view.ui, UiControlId::missionRow, 0, width, height);
      click(view.ui, UiControlId::missionAccept, -1, width, height); advance(4, now); return;
    case 4:
      if (view.ui.missionStage != 1) return;
      if (!capture("first-ore-accepted")) return;
      tap(SDLK_F1); advance(5, now); return;
    case 5:
      if (view.ui.screen != UiScreen::station) return;
      click(view.ui, UiControlId::stationLaunch, -1, width, height); advance(6, now); return;
    case 6:
      if (recoverForTravel()) return;
      if (view.ship.docked) { repairAndRelaunch(); return; }
      navigate(view, 0, 220);
      if (std::hypot(view.ship.x, view.ship.y - 280) < flight::kMineRange && flight::speed(view.ship) < 30) {
        setFlightKeys(false, false, false, true); tap(SDLK_e); nextAction_ = now + 0.6;
      }
      if (view.ship.cargo >= 1 && capture("mining")) advance(7, now);
      return;
    case 7:
      if (view.ship.destroyed) {
        setFlightKeys(false, false, false, true);
        tap(SDLK_r);
        advance(6, now);
        nextAction_ = now + 1.0;
        return;
      }
      navigate(view, 0, 55);
      if (std::hypot(view.ship.x, view.ship.y) < 80 && flight::speed(view.ship) < 30) {
        setFlightKeys(false, false, false, true); tap(SDLK_f); nextAction_ = now + 0.5;
      }
      if (view.ship.docked) { setFlightKeys(false, false, false, false); tap(SDLK_F5); advance(8, now); }
      return;
    case 8:
      if (view.ui.screen != UiScreen::mission) return;
      click(view.ui, UiControlId::missionRow, 0, width, height);
      click(view.ui, UiControlId::missionTurnIn, -1, width, height); advance(9, now); return;
    case 9:
      if (view.ui.missionStage != 2) {
        if (view.ui.screen == UiScreen::mission && !view.ui.commandPending && now >= nextAction_) {
          click(view.ui, UiControlId::missionRow, 0, width, height);
          click(view.ui, UiControlId::missionTurnIn, -1, width, height);
          nextAction_ = now + 2.0;
        }
        return;
      }
      if (!capture("first-ore-complete")) return;
      click(view.ui, UiControlId::missionRow, 1, width, height);
      click(view.ui, UiControlId::missionAccept, -1, width, height); advance(10, now); return;
    case 10:
      if (view.ui.career.supply != 1) return;
      if (!capture("supply-contract")) return;
      tap(SDLK_F1); advance(11, now); return;
    case 11:
      if (view.ui.screen != UiScreen::station) return;
      click(view.ui, UiControlId::stationLaunch, -1, width, height); advance(12, now); return;
    case 12:
      if (recoverForTravel()) return;
      if (view.ship.docked && view.ship.station == 1) {
        setFlightKeys(false, false, false, false); tap(SDLK_F4); advance(13, now); return;
      }
      if (view.ship.docked) { repairAndRelaunch(); return; }
      // Use the clear southern lane instead of crossing the central asteroid.
      if (view.ship.x < 580) navigate(view, 650, -100);
      else navigate(view, 650, 55);
      if (std::hypot(view.ship.x - 650, view.ship.y) < 80 && flight::speed(view.ship) < 30) {
        setFlightKeys(false, false, false, true); tap(SDLK_f); nextAction_ = now + 0.5;
      }
      return;
    case 13:
      if (view.ui.screen != UiScreen::market) return;
      click(view.ui, UiControlId::marketParts, 1, width, height);
      click(view.ui, UiControlId::marketQuantityUp, -1, width, height);
      click(view.ui, UiControlId::marketBuy, -1, width, height); advance(14, now); return;
    case 14:
      if (view.ship.parts < 2 || view.ui.career.purchased < 2) return;
      if (!capture("market")) return;
      tap(SDLK_F1); advance(15, now); return;
    case 15:
      if (view.ui.screen != UiScreen::station) return;
      click(view.ui, UiControlId::stationLaunch, -1, width, height); advance(16, now); return;
    case 16:
      if (recoverForTravel()) return;
      if (view.ship.docked && view.ship.station == 0) {
        setFlightKeys(false, false, false, false); tap(SDLK_F5); advance(17, now); return;
      }
      if (view.ship.docked) { repairAndRelaunch(); return; }
      if (view.ship.y > -60) navigate(view, 650, -100);
      else if (view.ship.x > 70) navigate(view, 0, -100);
      else navigate(view, 0, 55);
      if (std::hypot(view.ship.x, view.ship.y) < 80 && flight::speed(view.ship) < 30) {
        setFlightKeys(false, false, false, true); tap(SDLK_f); nextAction_ = now + 0.5;
      }
      return;
    case 17:
      if (view.ui.screen != UiScreen::mission) return;
      click(view.ui, UiControlId::missionRow, 1, width, height);
      click(view.ui, UiControlId::missionTurnIn, -1, width, height); advance(18, now); return;
    case 18:
      if (view.ui.career.supply != 2) {
        // A background profile/GalNet response can still be in flight after
        // docking. Wait for the UI command gate, then retry the idempotent
        // turn-in through the same hit regions a player uses.
        if (view.ui.screen == UiScreen::mission && !view.ui.commandPending && now >= nextAction_) {
          click(view.ui, UiControlId::missionRow, 1, width, height);
          click(view.ui, UiControlId::missionTurnIn, -1, width, height);
          nextAction_ = now + 2.0;
        }
        return;
      }
      tap(SDLK_F1); advance(19, now); return;
    case 19:
      if (view.ui.screen != UiScreen::station) return;
      click(view.ui, UiControlId::stationOutfitting, -1, width, height); advance(20, now); return;
    case 20:
      if (view.ui.screen != UiScreen::outfitting) return;
      click(view.ui, UiControlId::outfitRow, 6, width, height);
      click(view.ui, UiControlId::outfitBuy, -1, width, height); advance(21, now); return;
    case 21:
      if (std::find(view.ui.ownedModules.begin(), view.ui.ownedModules.end(), "pulse-laser") == view.ui.ownedModules.end()) return;
      click(view.ui, UiControlId::outfitRow, 6, width, height);
      click(view.ui, UiControlId::outfitFit, -1, width, height); advance(22, now); return;
    case 22:
      if (view.ui.fittedModules[3] != "pulse-laser") return;
      if (!capture("outfitting")) return;
      tap(SDLK_F5); advance(23, now); return;
    case 23:
      if (view.ui.screen != UiScreen::mission) return;
      click(view.ui, UiControlId::missionRow, 2, width, height);
      click(view.ui, UiControlId::missionAccept, -1, width, height); advance(24, now); return;
    case 24:
      if (view.ui.career.response != 1) return;
      tap(SDLK_F1); advance(25, now); return;
    case 25:
      if (view.ui.screen != UiScreen::station) return;
      click(view.ui, UiControlId::stationLaunch, -1, width, height); advance(26, now); return;
    case 26: {
      if (view.ui.career.complete) { setFlightKeys(false, false, false, true); advance(27, now); return; }
      if (view.ship.destroyed) {
        setFlightKeys(false, false, false, true);
        if (!capture("combat-destruction")) return;
        if (now >= nextAction_) { tap(SDLK_r); nextAction_ = now + 1.0; }
        return;
      }
      if (view.ship.docked) {
        if (view.ui.screen == UiScreen::station && now >= nextAction_) {
          click(view.ui, UiControlId::stationLaunch, -1, width, height);
          nextAction_ = now + 1.0;
        }
        return;
      }
      const auto target = std::find_if(view.contacts.begin(), view.contacts.end(), [](const flight::Contact& contact) { return contact.hostile; });
      if (target == view.contacts.end()) return;
      const double distance = std::hypot(target->x - view.ship.x, target->y - view.ship.y);
      if (distance > 220) navigate(view, target->x, target->y - 40);
      else {
        setFlightKeys(false, false, false, true);
        if (view.targetId.empty()) tap(SDLK_TAB);
        if (target->hull == target->maxHull && !capture("red-wake-target")) return;
        if (view.ship.weaponCooldown < 0.05 && now >= nextAction_) {
          std::cout << "ACCEPTANCE combat-fire target=" << view.targetId
                    << " target-hull=" << target->hull << " range=" << static_cast<int>(distance)
                    << " player-hull=" << view.ship.hull << std::endl;
          tap(SDLK_SPACE); nextAction_ = now + 1.05;
        }
        if (target->hull < target->maxHull && !capture("combat")) return;
      }
      return;
    }
    case 27:
      if (view.ui.screen != UiScreen::completion) return;
      if (!capture("career-completion")) return;
      click(view.ui, UiControlId::dismiss, -1, width, height); advance(28, now); return;
    case 28:
      if (recoverForTravel()) return;
      if (view.ship.docked) {
        setFlightKeys(false, false, false, false); tap(SDLK_F7); advance(29, now); return;
      }
      if (view.ui.screen != UiScreen::flight) {
        tap(SDLK_ESCAPE);
        nextAction_ = now + 0.5;
        return;
      }
      navigate(view, 0, 0);
      if (std::hypot(view.ship.x, view.ship.y) < 80 && flight::speed(view.ship) < 30) {
        setFlightKeys(false, false, false, true); tap(SDLK_f); nextAction_ = now + 0.5;
      }
      return;
    case 29:
      if (view.ui.screen != UiScreen::galnet || view.ui.galnet.size() < 7) return;
      if (!capture("galnet-complete")) return;
      done_ = true;
      std::cout << "ACCEPTANCE PASS phase=journey credits=" << view.credits
                << " xp=" << view.experience << " salvage=" << view.ui.salvage << '\n';
      return;
  }
}

} // namespace helion::client
