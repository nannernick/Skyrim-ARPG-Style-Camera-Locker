#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

// CommonLib only enables "..."sv literals inside its own namespaces, but the plugin-info
// file it generates uses them at global scope.
using namespace std::literals;
