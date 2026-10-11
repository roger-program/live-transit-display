#include "framebuffer.h"
#include <algorithm>

namespace transit {
Framebuffer::Framebuffer(int width, int height)
    : width_(width), height_(height), pixels_(static_cast<size_t>(width) * height) {}
void Framebuffer::clear(Color color) { std::fill(pixels_.begin(), pixels_.end(), color); }
void Framebuffer::pixel(int x, int y, Color color) {
  if (x >= 0 && x < width_ && y >= 0 && y < height_)
    pixels_[static_cast<size_t>(y) * width_ + x] = color;
}
Color Framebuffer::pixel(int x, int y) const {
  return x >= 0 && x < width_ && y >= 0 && y < height_
             ? pixels_[static_cast<size_t>(y) * width_ + x]
             : 0;
}
void Framebuffer::rect(int x, int y, int width, int height, Color color) {
  for (int yy = std::max(y, 0); yy < std::min(y + height, height_); ++yy)
    for (int xx = std::max(x, 0); xx < std::min(x + width, width_); ++xx)
      pixel(xx, yy, color);
}
} // namespace transit
