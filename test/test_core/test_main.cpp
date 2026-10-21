#include "badge_contrast.h"
#include "feed_request.h"
#include "fixture.h"
#include "renderer.h"
#include <ArduinoJson.h>
#include <unity.h>

using namespace transit;
void setUp() {}
void tearDown() {}
namespace {
Feed fixture() {
  Feed feed;
  TEST_ASSERT_TRUE(parseFeed(FixtureJson, feed) == ParseResult::Ok);
  return feed;
}
Snapshot sample() {
  Snapshot snapshot;
  snapshot.feed = fixture();
  snapshot.hasFeed = true;
  snapshot.receivedAtMs = 1000;
  return snapshot;
}
std::string edited(void (*edit)(JsonDocument &)) {
  DynamicJsonDocument doc(128 * 1024);
  deserializeJson(doc, FixtureJson);
  edit(doc);
  std::string out;
  serializeJson(doc, out);
  return out;
}
void protocol_fixture() {
  const auto feed = fixture();
  TEST_ASSERT_EQUAL_INT64(1791030107985LL, feed.generatedAt);
  TEST_ASSERT_EQUAL(2, feed.stops.size());
  TEST_ASSERT_EQUAL_STRING("6660122", feed.stops[0].id.c_str());
  TEST_ASSERT_EQUAL(4, feed.stops[0].groups.size());
  TEST_ASSERT_EQUAL_INT64(1791030248000LL, feed.stops[0].groups[0].arrivals[0].arrivalAt);
}
void forward_compatibility() {
  Feed feed;
  auto json = edited([](JsonDocument &doc) {
    doc["extra"] = 2;
    doc["stops"][0]["mode"] = "new-provider";
    doc["stops"][0]["stop"] = "opaque:001A";
    doc["stops"][0]["status"] = "future-status";
    doc["stops"][0]["future"] = true;
    doc["stops"][0]["groups"][0]["extra"] = true;
    doc["stops"][0]["groups"][0]["arrivals"][0]["debug"]["anything"] = 1;
  });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Ok);
  TEST_ASSERT_TRUE(feed.stops[0].status == StopStatus::Unavailable);
  Snapshot snapshot{feed, true, 1000, RequestError::None};
  auto view = viewAt(snapshot, feed.generatedAt, 1000, true);
  TEST_ASSERT_EQUAL(0, view.stops[0].groups.size());
  TEST_ASSERT_EQUAL(2, view.stops[1].groups.size());
  TEST_ASSERT_EQUAL_STRING("opaque:001A", view.stops[0].id.c_str());
}
void legacy_fields() {
  Feed feed;
  auto json = edited([](JsonDocument &doc) {
    doc["stops"][0].remove("realtimeStatus");
    doc["stops"][0]["name"] = nullptr;
    doc["stops"][0]["groups"][0]["arrivals"][0].remove("provenance");
  });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Ok);
  TEST_ASSERT_FALSE(feed.stops[0].nameKnown);
  TEST_ASSERT_TRUE(feed.stops[0].groups[0].arrivals[0].provenance == Provenance::Unknown);
}
void reject_versions_transactionally() {
  auto feed = fixture();
  const auto before = feed.generatedAt;
  TEST_ASSERT_TRUE(parseFeed("{\"version\":2}", feed) == ParseResult::UnsupportedVersion);
  TEST_ASSERT_TRUE(parseFeed("[]", feed) == ParseResult::Invalid);
  TEST_ASSERT_TRUE(parseFeed("broken", feed) == ParseResult::Invalid);
  TEST_ASSERT_TRUE(parseFeed(std::string(FixtureJson) + " trailing", feed) == ParseResult::Invalid);
  TEST_ASSERT_TRUE(parseFeed(std::string(FixtureJson) + "\n\t ", feed) == ParseResult::Ok);
  TEST_ASSERT_EQUAL_INT64(before, feed.generatedAt);
}
void reject_bad_fields() {
  Feed feed;
  auto json = edited([](JsonDocument &d) {
    d["stops"][0]["groups"][0]["arrivals"][0]["arrivalAt"] = "1791030248000";
  });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Invalid);
  json = edited(
      [](JsonDocument &d) { d["stops"][0]["groups"][0]["arrivals"][0]["provenance"] = "assumed"; });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Invalid);
  json = edited([](JsonDocument &d) { d["stops"][0]["groups"][0]["lineColor"] = "#fff"; });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Invalid);
  json = edited([](JsonDocument &d) { d["stops"][0].remove("name"); });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Invalid);
  json =
      edited([](JsonDocument &d) { d["stops"][0]["groups"][0]["arrivals"][1]["arrivalAt"] = 1; });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Invalid);
  json = edited([](JsonDocument &d) { d["generatedAt"] = 9007199254740992.0; });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Invalid);
  json = edited([](JsonDocument &d) { d["generatedAt"] = 1.5; });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Invalid);
}
void bounded_resources() {
  Feed feed;
  TEST_ASSERT_TRUE(parseFeed(std::string(MaxBodyBytes + 1, ' '), feed) ==
                   ParseResult::ResourceLimit);
  auto json =
      edited([](JsonDocument &d) { d["stops"][0]["name"] = std::string(MaxStringBytes + 1, 'a'); });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Invalid);
  json = edited([](JsonDocument &d) {
    auto stops = d["stops"].as<JsonArray>();
    for (unsigned i = 0; i < 21; ++i)
      stops.add(stops[0]);
  });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::ResourceLimit);
}
void preserve_identifiers_and_color() {
  Feed feed;
  auto json = edited([](JsonDocument &d) {
    d["stops"][0]["groups"][0]["lineColor"] = "#Ab12ef";
    d["stops"][0]["groups"][0]["arrivals"][0]["sourceIdentifier"]["kind"] = "vehicle";
    d["stops"][0]["groups"][0]["arrivals"][0]["sourceIdentifier"]["id"] = "001-A";
  });
  TEST_ASSERT_TRUE(parseFeed(json, feed) == ParseResult::Ok);
  TEST_ASSERT_EQUAL_STRING("#Ab12ef", feed.stops[0].groups[0].lineColor.c_str());
  TEST_ASSERT_EQUAL_STRING("001-A", feed.stops[0].groups[0].arrivals[0].identifier.c_str());
}
void countdown_boundaries() {
  Arrival a{100000, Provenance::Realtime, {}, {}};
  TEST_ASSERT_EQUAL_STRING("1M", countdown(a, 40000).c_str());
  TEST_ASSERT_EQUAL_STRING("DUE", countdown(a, 40001).c_str());
  TEST_ASSERT_EQUAL_STRING("DUE", countdown(a, 99999).c_str());
  TEST_ASSERT_TRUE(countdown(a, 100000).empty());
  a.provenance = Provenance::Scheduled;
  TEST_ASSERT_EQUAL_STRING("S1M", countdown(a, 40000).c_str());
  a.provenance = Provenance::Unknown;
  TEST_ASSERT_EQUAL_STRING("?DUE", countdown(a, 99999).c_str());
}
void freshness_boundary_and_monotonic_guard() {
  auto snapshot = sample();
  const auto n = snapshot.feed.generatedAt;
  TEST_ASSERT_TRUE(viewAt(snapshot, n + 119999, 120999, true).stops[0].status == StopStatus::Ok);
  auto stale = viewAt(snapshot, n + 120000, 121000, true);
  TEST_ASSERT_TRUE(stale.stops[0].status == StopStatus::Stale);
  TEST_ASSERT_EQUAL(0, stale.stops[0].groups.size());
  TEST_ASSERT_TRUE(viewAt(snapshot, n, 121000, true).stops[0].status == StopStatus::Stale);
  TEST_ASSERT_TRUE(viewAt(snapshot, n, 999, true).stops[0].status == StopStatus::Stale);
}
void clock_and_request_errors_suppress_arrivals() {
  auto snapshot = sample();
  TEST_ASSERT_EQUAL_STRING("CLOCK", viewAt(snapshot, 0, 1000, false).message.c_str());
  TEST_ASSERT_EQUAL_STRING(
      "CLOCK", viewAt(snapshot, snapshot.feed.generatedAt - 5001, 1000, true).message.c_str());
  for (auto error : {RequestError::Network, RequestError::Auth, RequestError::RateLimited,
                     RequestError::InvalidFeed}) {
    snapshot.error = error;
    auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
    TEST_ASSERT_TRUE(view.stops.empty());
    TEST_ASSERT_EQUAL_STRING(requestMessage(error), view.message.c_str());
  }
}
void partial_scheduled_and_expiry() {
  Feed feed;
  TEST_ASSERT_TRUE(parseFeed(StatesJson, feed) == ParseResult::Ok);
  Snapshot snapshot{feed, true, 1000, RequestError::None};
  auto view = viewAt(snapshot, feed.generatedAt, 1000, true);
  TEST_ASSERT_TRUE(view.stops[2].status == StopStatus::Partial);
  TEST_ASSERT_EQUAL_STRING(
      "SDUE", countdown(view.stops[3].groups[0].arrivals[0], feed.generatedAt).c_str());
  TEST_ASSERT_EQUAL(0, view.stops[7].groups.size());
  TEST_ASSERT_EQUAL(0, view.stops[8].groups.size());
  snapshot.feed.stops[3].groups[0].arrivals.resize(1);
  auto expired = viewAt(snapshot, feed.generatedAt + 59000, 60000, true);
  TEST_ASSERT_TRUE(expired.stops[3].status == StopStatus::Unavailable);
  TEST_ASSERT_TRUE(expired.stops[2].status == StopStatus::Partial);
  TEST_ASSERT_EQUAL(1, expired.stops[2].groups[0].arrivals.size());
}
void expiry_is_local_and_does_not_mutate_feed() {
  auto snapshot = sample();
  auto &arrivals = snapshot.feed.stops[0].groups[0].arrivals;
  arrivals[0].arrivalAt = snapshot.feed.generatedAt;
  auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  TEST_ASSERT_EQUAL(1, view.stops[0].groups[0].arrivals.size());
  TEST_ASSERT_EQUAL(2, arrivals.size());
}
void request_path_and_credentials() {
  TEST_ASSERT_EQUAL_STRING("/v1/arrivals?stop=tram%3Aends&stop=rail%3AA%26B%2B1",
                           arrivalsPath({"tram:ends", "rail:A&B+1"}).c_str());
  TEST_ASSERT_TRUE(validOrigin("https://feed.example:443"));
  for (const auto &origin : {"http://example", "https://user@host", "https://host?a=1",
                             "https://host/", "https://host\n"})
    TEST_ASSERT_FALSE(validOrigin(origin));
  TEST_ASSERT_TRUE(validBearer("synthetic.token+abc/="));
  TEST_ASSERT_FALSE(validBearer(""));
  TEST_ASSERT_FALSE(validBearer("a\r\nInjected: header"));
  TEST_ASSERT_FALSE(validBearer("a=b"));
}
void http_failures_and_backoff() {
  TEST_ASSERT_TRUE(readResponse(401, FixtureJson, "").error == RequestError::Auth);
  TEST_ASSERT_TRUE(readResponse(403, "ignored error body", "").error == RequestError::Auth);
  TEST_ASSERT_EQUAL(60000, readResponse(429, "{}", "30").retryMs);
  TEST_ASSERT_EQUAL(90000, readResponse(429, "{}", "90").retryMs);
  TEST_ASSERT_EQUAL(3600000, readResponse(503, "{}", "99999999").retryMs);
  TEST_ASSERT_TRUE(readResponse(302, FixtureJson, "").error == RequestError::Http);
  TEST_ASSERT_TRUE(readResponse(204, "", "").error == RequestError::InvalidFeed);
  TEST_ASSERT_TRUE(readResponse(200, FixtureJson, "").error == RequestError::None);
}
void requested_order_and_missing_stop() {
  auto feed = selectStops(fixture(), {"bus:94", "fgc:opaque", "metro:6660122", "bus:94"});
  TEST_ASSERT_EQUAL(4, feed.stops.size());
  TEST_ASSERT_EQUAL_STRING("94", feed.stops[0].id.c_str());
  TEST_ASSERT_TRUE(feed.stops[1].status == StopStatus::Unavailable);
  TEST_ASSERT_EQUAL_STRING("opaque", feed.stops[1].id.c_str());
  TEST_ASSERT_EQUAL(4, feed.stops[2].groups.size());
}
void pagination_capacity_and_order() {
  auto snapshot = sample();
  const auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  const auto small = pagesFor(view, 32);
  const auto large = pagesFor(view, 64);
  TEST_ASSERT_EQUAL(6, small.size());
  TEST_ASSERT_EQUAL(3, large.size());
  TEST_ASSERT_EQUAL(0, small[3].stopIndex);
  TEST_ASSERT_EQUAL(1, small[4].stopIndex);
  TEST_ASSERT_EQUAL(3, small[3].groupStart);
  TEST_ASSERT_EQUAL(2, large[1].groupCount);
  TEST_ASSERT_EQUAL(4, small[0].servicePages);
}
void label_reveal_and_utf8() {
  TEST_ASSERT_EQUAL_STRING("ESTACIO GRACIA ?", pixelText("Estació Gràcia 🚆").c_str());
  TEST_ASSERT_EQUAL_STRING("ESTACIO", pixelText("Estacio\xcc\x81").c_str());
  TEST_ASSERT_EQUAL(0, overflowOffset(80, 60, 700));
  TEST_ASSERT_EQUAL(20, overflowOffset(80, 60, 1300));
  TEST_ASSERT_EQUAL(20, overflowOffset(80, 60, 2000));
  TEST_ASSERT_EQUAL(0, overflowOffset(80, 60, 2600));
  TEST_ASSERT_EQUAL(0, overflowOffset(40, 60, 1500));
}
uint64_t pixelsHash(const Framebuffer &frame) {
  uint64_t hash = 14695981039346656037ULL;
  for (int y = 0; y < frame.height(); ++y)
    for (int x = 0; x < frame.width(); ++x) {
      hash ^= frame.pixel(x, y);
      hash *= 1099511628211ULL;
    }
  return hash;
}
void raster_determinism_and_paging() {
  auto snapshot = sample();
  auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  ArrivalRenderer renderer;
  Framebuffer a(64, 32), b(64, 32);
  renderer.render(a, view, snapshot.feed.generatedAt, 0);
  renderer.render(b, view, snapshot.feed.generatedAt, 0);
  TEST_ASSERT_EQUAL_UINT64(pixelsHash(a), pixelsHash(b));
  const auto initial = pixelsHash(a);
  renderer.render(b, view, snapshot.feed.generatedAt, 3300);
  TEST_ASSERT_NOT_EQUAL(initial, pixelsHash(b));
  for (int x = 0; x < 64; ++x) {
    TEST_ASSERT_EQUAL(0, a.pixel(x, 0));
    TEST_ASSERT_EQUAL(0, a.pixel(x, 31));
  }
  renderer.render(b, view, snapshot.feed.generatedAt, 3150);
  for (int y = 0; y < 10; ++y)
    for (int x = 0; x < 64; ++x)
      TEST_ASSERT_EQUAL(a.pixel(x, y), b.pixel(x, y));
  renderer.render(b, view, snapshot.feed.generatedAt, 3 * 3300 + 1500);
  TEST_ASSERT_NOT_EQUAL(initial, pixelsHash(b));
  renderer.render(b, view, snapshot.feed.generatedAt, 6 * 3300);
  TEST_ASSERT_EQUAL_UINT64(initial, pixelsHash(b));
}
void diagnostics_and_clipping() {
  Framebuffer frame(64, 32);
  TEST_ASSERT_EQUAL(16, renderDiagnostic(frame, Diagnostic::Red, 0));
  TEST_ASSERT_EQUAL_HEX32(0xff0000, frame.pixel(63, 31));
  renderDiagnostic(frame, Diagnostic::Green, 0);
  TEST_ASSERT_EQUAL_HEX32(0x00ff00, frame.pixel(0, 0));
  renderDiagnostic(frame, Diagnostic::Blue, 0);
  TEST_ASSERT_EQUAL_HEX32(0x0000ff, frame.pixel(0, 0));
  renderDiagnostic(frame, Diagnostic::Rows, 200);
  TEST_ASSERT_EQUAL_HEX32(0xffffff, frame.pixel(63, 1));
  renderDiagnostic(frame, Diagnostic::Columns, 200);
  TEST_ASSERT_EQUAL_HEX32(0xffffff, frame.pixel(1, 31));
  for (unsigned i = 0; i < 4; ++i)
    TEST_ASSERT_EQUAL(8 << i, renderDiagnostic(frame, Diagnostic::Brightness, i * 2000));
  frame.pixel(-1, 0, 0xffffff);
  frame.pixel(64, 32, 0xffffff);
  TEST_ASSERT_EQUAL(0, frame.pixel(-1, 0));
  TEST_ASSERT_EQUAL(0, frame.pixel(64, 32));
}
void fixture_line_colors() {
  auto snapshot = sample();
  auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  ArrivalRenderer renderer;
  Framebuffer frame(64, 32);
  constexpr Color expected[] = {0xce1126, 0xce1126, 0x1eb53a, 0x1eb53a, 0x42ac52, 0x42ac52};
  for (size_t page = 0; page < 6; ++page) {
    renderer.render(frame, view, snapshot.feed.generatedAt, page * PageDurationMs);
    TEST_ASSERT_EQUAL_HEX32(expected[page], frame.pixel(1, 11));
  }
  TEST_ASSERT_TRUE(snapshot.feed.stops[1].groups[0].lineColor.empty());
  TEST_ASSERT_TRUE(view.stops[1].groups[0].lineColor.empty());
}
void explicit_color_precedes_fallback() {
  auto snapshot = sample();
  const auto json = edited([](JsonDocument &doc) {
    doc["stops"][0]["mode"] = "future-mode";
    doc["stops"][0]["groups"][0]["lineColor"] = "#Ab12ef";
    doc["stops"][1]["groups"][0]["lineColor"] = "#000000";
  });
  TEST_ASSERT_TRUE(parseFeed(json, snapshot.feed) == ParseResult::Ok);
  const auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  ArrivalRenderer renderer;
  Framebuffer frame(64, 32);
  renderer.render(frame, view, snapshot.feed.generatedAt, 0);
  TEST_ASSERT_EQUAL_HEX32(0xab12ef, frame.pixel(1, 11));
  renderer.render(frame, view, snapshot.feed.generatedAt, 4 * PageDurationMs);
  TEST_ASSERT_EQUAL_HEX32(0x000000, frame.pixel(1, 11));
}
void missing_color_stays_neutral() {
  auto snapshot = sample();
  const auto json = edited([](JsonDocument &doc) {
    doc["stops"][0]["groups"][0].remove("lineColor");
    doc["stops"][1]["mode"] = "future-mode";
  });
  TEST_ASSERT_TRUE(parseFeed(json, snapshot.feed) == ParseResult::Ok);
  const auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  ArrivalRenderer renderer;
  Framebuffer frame(64, 32);
  renderer.render(frame, view, snapshot.feed.generatedAt, 0);
  TEST_ASSERT_EQUAL_HEX32(0x777777, frame.pixel(1, 11));
  renderer.render(frame, view, snapshot.feed.generatedAt, 4 * PageDurationMs);
  TEST_ASSERT_EQUAL_HEX32(0x777777, frame.pixel(1, 11));
  TEST_ASSERT_EQUAL_HEX32(0x777777, frame.pixel(1, 1));
}
void framebuffer_preserves_rgb() {
  Framebuffer frame(64, 32);
  frame.clear(0x123456);
  frame.pixel(2, 3, 0xab12ef);
  frame.rect(4, 5, 2, 2, 0x42ac52);
  TEST_ASSERT_EQUAL_HEX32(0x123456, frame.pixel(63, 31));
  TEST_ASSERT_EQUAL_HEX32(0xab12ef, frame.pixel(2, 3));
  TEST_ASSERT_EQUAL_HEX32(0x42ac52, frame.pixel(5, 6));
  TEST_ASSERT_EQUAL_HEX32(0x123456, frame.pixel(3, 5));
}
void badge_contrast_pairs() {
  Framebuffer frame(64, 32);
  constexpr Color backgrounds[] = {0xce1126, 0x93248f, 0x1eb53a, 0xf7a30e,
                                   0x005a97, 0xfb712b, 0x00a6d6, 0x89b94c};
  TEST_ASSERT_EQUAL(8, badgecontrast::Count);
  for (unsigned index = 0; index < badgecontrast::Count; ++index) {
    for (bool swapped : {false, true}) {
      badgecontrast::render(frame, index, swapped);
      unsigned glyphPixels = 0;
      for (int y = 10; y < 21; ++y)
        for (int dx = 0; dx < 29; ++dx) {
          const auto left = frame.pixel(1 + dx, y);
          const auto right = frame.pixel(34 + dx, y);
          if (left == backgrounds[index]) {
            TEST_ASSERT_EQUAL_HEX32(backgrounds[index], right);
          } else {
            TEST_ASSERT_EQUAL_HEX32(swapped ? 0xffffff : 0, left);
            TEST_ASSERT_EQUAL_HEX32(swapped ? 0 : 0xffffff, right);
            ++glyphPixels;
          }
        }
      TEST_ASSERT_TRUE(glyphPixels > 0);
      TEST_ASSERT_EQUAL_HEX32(backgrounds[index], frame.pixel(1, 10));
      for (int y = 0; y < 32; ++y) {
        TEST_ASSERT_EQUAL(0, frame.pixel(0, y));
        TEST_ASSERT_EQUAL(0, frame.pixel(63, y));
      }
    }
  }
}
void service_page_indicators() {
  auto snapshot = sample();
  auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  Framebuffer frame(64, 32);
  ArrivalRenderer renderer;
  renderer.render(frame, view, snapshot.feed.generatedAt, 0);
  for (int i = 0; i < 4; ++i)
    TEST_ASSERT_EQUAL_HEX32(i == 0 ? 0xaaff66 : 0x384438, frame.pixel(1 + 3 * i, 30));
  renderer.render(frame, view, snapshot.feed.generatedAt, PageDurationMs);
  for (int i = 0; i < 4; ++i)
    TEST_ASSERT_EQUAL_HEX32(i == 1 ? 0xaaff66 : 0x384438, frame.pixel(1 + 3 * i, 30));
  view.stops.resize(1);
  view.stops[0].groups.resize(1);
  renderer.render(frame, view, snapshot.feed.generatedAt, 0);
  for (int x = 0; x < 64; ++x)
    TEST_ASSERT_EQUAL(0, frame.pixel(x, 30));
}
void reviewed_raster_regression() {
  auto snapshot = sample();
  auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  ArrivalRenderer renderer;
  Framebuffer frame(64, 32);
  // Keep the reviewed neutral-colour geometry baseline; separate tests cover new colours.
  for (auto &stop : view.stops)
    for (auto &group : stop.groups)
      group.lineColor = "#777777";
  // Baselines from the reviewed native PNGs; catch missing/overlapping text and markers.
  renderer.render(frame, view, snapshot.feed.generatedAt, 0);
  TEST_ASSERT_EQUAL_UINT64(17682094639170009042ULL, pixelsHash(frame));
  renderer.render(frame, view, snapshot.feed.generatedAt, 5 * PageDurationMs);
  TEST_ASSERT_EQUAL_UINT64(17687063311361716670ULL, pixelsHash(frame));
  TEST_ASSERT_TRUE(parseFeed(StatesJson, snapshot.feed) == ParseResult::Ok);
  view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  renderer.render(frame, view, snapshot.feed.generatedAt, 2 * PageDurationMs);
  TEST_ASSERT_EQUAL_UINT64(2637198088624226296ULL, pixelsHash(frame));
  renderer.render(frame, view, snapshot.feed.generatedAt, 3 * PageDurationMs);
  TEST_ASSERT_EQUAL_UINT64(4873872969905638214ULL, pixelsHash(frame));
}
} // namespace
int main() {
  UNITY_BEGIN();
  RUN_TEST(protocol_fixture);
  RUN_TEST(forward_compatibility);
  RUN_TEST(legacy_fields);
  RUN_TEST(reject_versions_transactionally);
  RUN_TEST(reject_bad_fields);
  RUN_TEST(bounded_resources);
  RUN_TEST(preserve_identifiers_and_color);
  RUN_TEST(countdown_boundaries);
  RUN_TEST(freshness_boundary_and_monotonic_guard);
  RUN_TEST(clock_and_request_errors_suppress_arrivals);
  RUN_TEST(partial_scheduled_and_expiry);
  RUN_TEST(expiry_is_local_and_does_not_mutate_feed);
  RUN_TEST(request_path_and_credentials);
  RUN_TEST(http_failures_and_backoff);
  RUN_TEST(requested_order_and_missing_stop);
  RUN_TEST(pagination_capacity_and_order);
  RUN_TEST(label_reveal_and_utf8);
  RUN_TEST(raster_determinism_and_paging);
  RUN_TEST(diagnostics_and_clipping);
  RUN_TEST(fixture_line_colors);
  RUN_TEST(explicit_color_precedes_fallback);
  RUN_TEST(missing_color_stays_neutral);
  RUN_TEST(framebuffer_preserves_rgb);
  RUN_TEST(badge_contrast_pairs);
  RUN_TEST(service_page_indicators);
  RUN_TEST(reviewed_raster_regression);
  return UNITY_END();
}
