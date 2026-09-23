#pragma once
#include "client/render.h"
#include <SDL.h>

namespace helion::client {
// Private input draft. Password bytes never enter View or a render snapshot.
// SDL events and keyboard shortcuts resolve through the same activation path.
class UiInput {
 public:
  std::string handle(View&, const SDL_Event&, int windowWidth, int windowHeight);
  void clearCredentials();
 private:
  std::string username_, password_, display_;
  UiControlId field_ = UiControlId::accountName;
  bool leftDown_ = false;
  std::string activate(View&, UiHitResult);
  void publishDraft(UiState&) const;
};
}
