#include "feed_parser.h"
#include "framebuffer.h"
#include "renderer.h"
#include "state.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

void save(const transit::Framebuffer &frame, const std::string &path) {
  std::ofstream file(path, std::ios::binary);
  file << "P6\n" << frame.width() << " " << frame.height() << "\n255\n";
  for (int y = 0; y < frame.height(); ++y)
    for (int x = 0; x < frame.width(); ++x) {
      auto pixel = frame.pixel(x, y);
      char rgb[] = {static_cast<char>(pixel >> 16), static_cast<char>(pixel >> 8),
                    static_cast<char>(pixel)};
      file.write(rgb, 3);
    }
}
int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: render_preview fixture.json output-directory\n";
    return 1;
  }
  std::ifstream file(argv[1]);
  const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  transit::Snapshot snapshot;
  if (transit::parseFeed(json, snapshot.feed) != transit::ParseResult::Ok)
    return 2;
  snapshot.hasFeed = true;
  snapshot.receivedAtMs = 1000;
  std::filesystem::create_directories(argv[2]);
  transit::ArrivalRenderer renderer;
  for (const int width : {64, 128}) {
    transit::Framebuffer frame(width, width / 2);
    auto view = transit::viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
    const auto pages = transit::pagesFor(view, frame.height());
    for (size_t page = 0; page < pages.size(); ++page)
      for (const uint64_t phase : {uint64_t(0), uint64_t(1500), uint64_t(3150)}) {
        renderer.render(frame, view, snapshot.feed.generatedAt,
                        page * transit::PageDurationMs + phase);
        save(frame, std::string(argv[2]) + "/" + std::to_string(width) + "-page" +
                        std::to_string(page) + "-" + std::to_string(phase) + ".ppm");
      }
  }
  return 0;
}
