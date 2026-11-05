#pragma once

#include "MenuHelper/MenuHelper.h"

extern "C" __declspec(dllexport) const MenuHelper_API* MenuHelper_GetAPI(std::uint32_t a_requestedVersion);
