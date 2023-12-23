#include "AndroidEmulatedController.h"

std::array<std::unique_ptr<AndroidEmulatedController>, InputManager::kMaxController> AndroidEmulatedController::s_emulatedControllers;
