#pragma once
#include "model.h"
#include <string_view>

namespace transit {
enum class ParseResult { Ok, Invalid, UnsupportedVersion, ResourceLimit };
// Transactional: failures leave output untouched. Raw JSON/errors are never logged.
ParseResult parseFeed(std::string_view json, Feed &output);
} // namespace transit
