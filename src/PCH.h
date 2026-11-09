#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <functional>
#include <fstream>
#include <numbers>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace logs = SKSE::log;

// CommonLib only enables "..."sv literals inside its own namespaces, but the plugin-info
// file it generates uses them at global scope.
using namespace std::literals;
