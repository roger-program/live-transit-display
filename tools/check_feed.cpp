// Host-only protocol/renderer check. Never connects to a physical display.
#include "feed_parser.h"
#include "renderer.h"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  std::ifstream input(argv[1]);
  if (!input)
    return 2;
  const std::string json((std::istreambuf_iterator<char>(input)), {});
  transit::Snapshot snapshot;
  if (transit::parseFeed(json, snapshot.feed) != transit::ParseResult::Ok)
    return 1;
  snapshot.hasFeed = true;
  snapshot.receivedAtMs = 1000;
  const auto view = transit::viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  const auto pages = transit::pagesFor(view, 32);
  transit::ArrivalRenderer renderer;
  transit::Framebuffer frame(64, 32);
  for (size_t i = 0; i < pages.size(); ++i) {
    renderer.render(frame, view, snapshot.feed.generatedAt, i * transit::PageDurationMs);
    if (view.stops[pages[i].stopIndex].operatorId != "FGC" || frame.pixel(1, 1) != 0x00834a)
      return 1;
  }
  std::cout << "Feed v1 parsed/rendered: " << view.stops.size() << " stops, " << pages.size()
            << " pages; FGC operator pixels verified\n";
}
