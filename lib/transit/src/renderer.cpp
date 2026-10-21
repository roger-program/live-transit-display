#include "renderer.h"
#include "font.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace transit {
namespace {
constexpr Color White = 0xe8eee8, Green = 0xaaff66, Grey = 0x777777, BusGreen = 0x42ac52;
const uint8_t *glyph(char character) {
  for (const auto &item : Font)
    if (item.character == character)
      return item.rows;
  for (const auto &item : Font)
    if (item.character == '?')
      return item.rows;
  return Font[0].rows;
}
const char *stopMessage(StopStatus status) {
  if (status == StopStatus::Stale)
    return "STALE";
  if (status == StopStatus::Ok || status == StopStatus::Empty || status == StopStatus::Partial)
    return "NO PRED";
  return "UNAVAIL";
}
Color lineColor(const std::string &color, const std::string &mode) {
  if (!color.empty())
    return static_cast<Color>(std::strtoul(color.c_str() + 1, nullptr, 16));
  // Presentation fallback only: never infer or populate canonical line identity.
  return mode == "bus" ? BusGreen : Grey;
}
void badge(Framebuffer &frame, const std::string &label, int x, int y, Color color, int offset,
           const std::string &textColor = "") {
  // Explicit width limit protects the separate countdown field from long line labels.
  int width = std::min(frame.width() / 2 - 2, textWidth(label) + 2);
  frame.rect(x + offset, y, width, 7, color);
  const Color ink =
      !textColor.empty() ? static_cast<Color>(std::strtoul(textColor.c_str() + 1, nullptr, 16))
      : (((color >> 16) & 255) * 299 + ((color >> 8) & 255) * 587 + (color & 255) * 114) > 150000
          ? 0
          : 0xffffff;
  drawText(frame, label, x + 1 + offset, y + 1, ink, std::max(1, x + offset),
           std::min(frame.width() - 1, x + offset + width));
}
void drawPage(Framebuffer &frame, const View &view, const Page &page, int64_t now,
              uint64_t pageTime, uint64_t stopTime, int offset, bool header, bool content) {
  const auto &stop = view.stops[page.stopIndex];
  if (header) {
    const std::string mode = stop.mode == "metro" ? "M" : stop.mode == "bus" ? "B" : "?";
    badge(frame, mode, 1, 1,
          stop.mode == "metro" ? 0xe53935
          : stop.mode == "bus" ? BusGreen
                               : Grey,
          offset);
    const std::string name = stop.nameKnown ? stop.name : stop.mode + ":" + stop.id;
    const int right = frame.width() - 1 - (stop.status == StopStatus::Partial ? 6 : 0);
    drawText(frame, name, 10 + offset, 2, White, std::max(1, 10 + offset),
             std::min(frame.width() - 1, right + offset),
             overflowOffset(textWidth(name), right - 10, stopTime));
    if (stop.status == StopStatus::Partial)
      drawText(frame, "!", right + offset, 2, 0xffaa00, 1, frame.width() - 1);
    frame.rect(1 + offset, 9, frame.width() - 2, 1, 0x384438);
  }
  if (!content)
    return;
  if (!page.groupCount) {
    drawText(frame, stopMessage(stop.status), 2 + offset, 16, Green, 1, frame.width() - 1);
    return;
  }
  const int spacing = frame.height() >= 54 ? 23 : 19;
  for (size_t i = 0; i < page.groupCount; ++i) {
    const auto &group = stop.groups[page.groupStart + i];
    const int y = 12 + static_cast<int>(i) * spacing;
    badge(frame, group.line.empty() ? "?" : group.line, 1, y - 1,
          lineColor(group.lineColor, stop.mode), offset, group.lineTextColor);
    std::string arrivals;
    for (size_t a = 0; a < std::min(size_t(2), group.arrivals.size()); ++a) {
      if (a)
        arrivals += ' ';
      arrivals += countdown(group.arrivals[a], now);
    }
    if (arrivals.empty())
      arrivals = "NO PRED";
    const int badgeWidth =
        std::min(frame.width() / 2 - 2, textWidth(group.line.empty() ? "?" : group.line) + 2);
    const int slotLeft = 1 + badgeWidth + 2;
    const int available = frame.width() - 1 - slotLeft;
    // If two values cannot fit, retain the first intact instead of clipping ambiguity.
    if (textWidth(arrivals) > available && group.arrivals.size() > 1)
      arrivals = countdown(group.arrivals.front(), now);
    if (textWidth(arrivals) > available)
      arrivals = group.arrivals.empty()
                     ? "NONE"
                     : (group.arrivals.front().provenance == Provenance::Scheduled ? "S99+M"
                        : group.arrivals.front().provenance == Provenance::Unknown ? "?99+M"
                                                                                   : "99+M");
    const int arrivalLeft = std::max(slotLeft, frame.width() - 1 - textWidth(arrivals));
    drawText(frame, arrivals, arrivalLeft + offset, y, Green, std::max(1, arrivalLeft + offset),
             std::min(frame.width() - 1, frame.width() - 1 + offset));
    drawText(frame, group.destination, 2 + offset, y + 9, White, std::max(1, 2 + offset),
             std::min(frame.width() - 1, frame.width() - 1 + offset),
             overflowOffset(textWidth(group.destination), frame.width() - 3, pageTime));
  }
  if (page.servicePages > 1) {
    const int count = std::min(static_cast<int>(page.servicePages), frame.width() / 3);
    for (int i = 0; i < count; ++i)
      frame.pixel(1 + i * 3 + offset, frame.height() - 2,
                  size_t(i) == page.servicePage ? Green : 0x384438);
  }
}
} // namespace

std::string pixelText(const std::string &utf8) {
  // Narrow embedded glyph policy: Catalan/Spanish accents transliterate for 5-row text.
  // Stored normalized labels are untouched. Unknown UTF-8 codepoints become one '?'.
  std::string out;
  for (size_t i = 0; i < utf8.size();) {
    const unsigned char c = utf8[i++];
    if (c < 128) {
      char upper = c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : static_cast<char>(c);
      bool known = false;
      for (const auto &g : Font)
        known = known || g.character == upper;
      out += known ? upper : '?';
      continue;
    }
    if (i < utf8.size() && ((c == 0xcc && static_cast<unsigned char>(utf8[i]) >= 0x80 &&
                             static_cast<unsigned char>(utf8[i]) <= 0xbf) ||
                            (c == 0xcd && static_cast<unsigned char>(utf8[i]) >= 0x80 &&
                             static_cast<unsigned char>(utf8[i]) <= 0xaf))) {
      ++i; // Combining accents: the preceding base glyph has already been retained.
      continue;
    }
    unsigned length = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : 2;
    char replacement = '?';
    if (c == 0xc3 && i < utf8.size()) {
      const unsigned char next = utf8[i];
      if ((next >= 0x80 && next <= 0x85) || (next >= 0xa0 && next <= 0xa5))
        replacement = 'A';
      if (next == 0x87 || next == 0xa7)
        replacement = 'C';
      if ((next >= 0x88 && next <= 0x8b) || (next >= 0xa8 && next <= 0xab))
        replacement = 'E';
      if ((next >= 0x8c && next <= 0x8f) || (next >= 0xac && next <= 0xaf))
        replacement = 'I';
      if (next == 0x91 || next == 0xb1)
        replacement = 'N';
      if ((next >= 0x92 && next <= 0x96) || (next >= 0xb2 && next <= 0xb6))
        replacement = 'O';
      if ((next >= 0x99 && next <= 0x9c) || (next >= 0xb9 && next <= 0xbc))
        replacement = 'U';
    }
    i = std::min(utf8.size(), i + length - 1);
    out += replacement;
  }
  return out;
}
int textWidth(const std::string &utf8) {
  const auto label = pixelText(utf8);
  int right = 0;
  for (size_t i = 0; i < label.size(); ++i)
    for (int y = 0; y < 5; ++y)
      for (int x = 0; x < 5; ++x)
        if (glyph(label[i])[y] & (1 << (4 - x)))
          right = std::max(right, static_cast<int>(i) * 6 + x + 1);
  return right;
}
int overflowOffset(int inkWidth, int viewportWidth, uint64_t elapsed) {
  int maximum = std::max(0, inkWidth - viewportWidth);
  if (!maximum || elapsed <= 700 || elapsed >= 2600)
    return 0;
  if (elapsed >= 1300 && elapsed <= 2000)
    return maximum;
  double progress = elapsed < 1300 ? (elapsed - 700) / 600.0 : (elapsed - 2000) / 600.0;
  double ease = progress * progress * (3 - 2 * progress);
  return static_cast<int>(std::round(maximum * (elapsed < 1300 ? ease : 1 - ease)));
}
void drawText(Framebuffer &frame, const std::string &text, int x, int y, Color color, int clipLeft,
              int clipRight, int offsetX) {
  const auto label = pixelText(text);
  for (size_t i = 0; i < label.size(); ++i) {
    const auto *rows = glyph(label[i]);
    for (int yy = 0; yy < 5; ++yy)
      for (int xx = 0; xx < 5; ++xx) {
        int px = x + static_cast<int>(i) * 6 + xx - offsetX;
        if (px >= clipLeft && px < clipRight && (rows[yy] & (1 << (4 - xx))))
          frame.pixel(px, y + yy, color);
      }
  }
}
std::vector<Page> pagesFor(const View &view, int height) {
  const size_t capacity = height >= 54 ? 2 : 1;
  std::vector<Page> pages;
  for (size_t s = 0; s < view.stops.size(); ++s) {
    const size_t groups = view.stops[s].groups.size();
    const size_t count = std::max(size_t(1), (groups + capacity - 1) / capacity);
    for (size_t p = 0; p < count; ++p)
      pages.push_back(
          {s, p * capacity, std::min(capacity, groups - std::min(groups, p * capacity)), p, count});
  }
  return pages;
}
void ArrivalRenderer::render(Framebuffer &frame, const View &view, int64_t now, uint64_t elapsed) {
  frame.clear();
  const auto pages = pagesFor(view, frame.height());
  if (pages.empty()) {
    drawText(frame, view.message, 2, 12, Green, 1, frame.width() - 1);
    return;
  }
  const size_t index = (elapsed / PageDurationMs) % pages.size();
  const size_t nextIndex = (index + 1) % pages.size();
  const auto &page = pages[index];
  const auto &next = pages[nextIndex];
  const uint64_t time = elapsed % PageDurationMs;
  const uint64_t stopTime = time + page.servicePage * PageDurationMs;
  int shift =
      pages.size() <= 1 || time < 3000 ? 0 : static_cast<int>((time - 3000) * frame.width() / 300);
  const bool sameStop = page.stopIndex == next.stopIndex;
  if (shift && sameStop) {
    drawPage(frame, view, page, now, time, stopTime, 0, true, false);
    drawPage(frame, view, page, now, time, stopTime, -shift, false, true);
    drawPage(frame, view, next, now, 0, stopTime, frame.width() - shift, false, true);
  } else {
    drawPage(frame, view, page, now, time, stopTime, -shift, true, true);
    if (shift)
      drawPage(frame, view, next, now, 0, 0, frame.width() - shift, true, true);
  }
}
uint8_t renderDiagnostic(Framebuffer &frame, Diagnostic mode, uint64_t elapsed) {
  frame.clear();
  switch (mode) {
  case Diagnostic::Red:
    frame.clear(0xff0000);
    break;
  case Diagnostic::Green:
    frame.clear(0x00ff00);
    break;
  case Diagnostic::Blue:
    frame.clear(0x0000ff);
    break;
  case Diagnostic::Pixels:
    for (int y = 0; y < frame.height(); ++y)
      for (int x = 0; x < frame.width(); ++x)
        if ((x + y) % 2 == 0)
          frame.pixel(x, y, 0xffffff);
    frame.pixel(static_cast<int>(elapsed / 100 % frame.width()),
                static_cast<int>(elapsed / 100 / frame.width() % frame.height()), 0xff0000);
    break;
  case Diagnostic::Rows:
    frame.rect(0, static_cast<int>(elapsed / 200 % frame.height()), frame.width(), 1, 0xffffff);
    break;
  case Diagnostic::Columns:
    frame.rect(static_cast<int>(elapsed / 200 % frame.width()), 0, 1, frame.height(), 0xffffff);
    break;
  case Diagnostic::Text:
    drawText(frame, "HELLO", 2, 2, 0xffffff, 0, frame.width());
    drawText(frame, std::to_string(frame.width()) + "X" + std::to_string(frame.height()), 2, 12,
             Green, 0, frame.width());
    frame.pixel(0, 0, 0xff0000);
    frame.pixel(frame.width() - 1, frame.height() - 1, 0x0000ff);
    break;
  case Diagnostic::Brightness: {
    constexpr uint8_t levels[] = {8, 16, 32, 64};
    const uint8_t level = levels[(elapsed / 2000) % 4];
    frame.clear(0xffffff);
    drawText(frame, std::to_string(level), 2, 2, 0, 0, frame.width());
    return level;
  }
  }
  return 16;
}
} // namespace transit
