#pragma once
#include <cstdint>
#include <vector>

namespace transit {
using Color = uint32_t; // RGB888; driver chooses its physical representation.
class Framebuffer {
public:
  Framebuffer(int width, int height);
  int width() const { return width_; }
  int height() const { return height_; }
  void clear(Color color = 0);
  void pixel(int x, int y, Color color);
  Color pixel(int x, int y) const;
  void rect(int x, int y, int width, int height, Color color);

private:
  int width_, height_;
  std::vector<Color> pixels_;
};
class DisplayDriver {
public:
  virtual ~DisplayDriver() = default;
  virtual bool begin() = 0;
  virtual void present(const Framebuffer &frame, uint8_t brightness) = 0;
};
} // namespace transit
