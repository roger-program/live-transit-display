#pragma once
#include "framebuffer.h"
#include "state.h"

namespace transit {
constexpr uint64_t PageDurationMs = 3300;
struct Page {
  size_t stopIndex = 0;
  size_t groupStart = 0;
  size_t groupCount = 0;
  size_t servicePage = 0;
  size_t servicePages = 1;
};
std::vector<Page> pagesFor(const View &view, int height);
std::string pixelText(const std::string &utf8);
int textWidth(const std::string &utf8);
int overflowOffset(int inkWidth, int viewportWidth, uint64_t elapsed);
void drawText(Framebuffer &frame, const std::string &text, int x, int y, Color color, int clipLeft,
              int clipRight, int offsetX = 0);
class Renderer {
public:
  virtual ~Renderer() = default;
  virtual void render(Framebuffer &frame, const View &view, int64_t now, uint64_t elapsed) = 0;
};
class ArrivalRenderer final : public Renderer {
public:
  void render(Framebuffer &frame, const View &view, int64_t now, uint64_t elapsed) override;
};
enum class Diagnostic { Red, Green, Blue, Pixels, Rows, Columns, Text, Brightness };
constexpr unsigned DiagnosticCount = 8;
uint8_t renderDiagnostic(Framebuffer &frame, Diagnostic mode, uint64_t elapsed);
} // namespace transit
