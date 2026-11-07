#pragma once
#include "framebuffer.h"
#include <string>

namespace transit {
struct OperatorSymbol {
  const char *id;
  uint8_t width, height;
  uint8_t rows[7];
  Color background, foreground;
};
// Reviewed local assets, not provider routing/colour policy. Add operators as
// data. FGC is the unchanged 7x5 mask from the private simulator's
// user-reference study.
inline constexpr OperatorSymbol OperatorSymbols[] = {
    {"FGC", 7, 5, {22, 41, 85, 74, 52, 0, 0}, 0xa1d341, 0xffffff},
};
inline const OperatorSymbol *operatorSymbolFor(const std::string &id) {
  for (const auto &symbol : OperatorSymbols)
    if (id == symbol.id)
      return &symbol;
  return nullptr;
}
} // namespace transit
