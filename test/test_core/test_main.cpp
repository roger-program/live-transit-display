#include "badge_contrast.h"
#include "brightness.h"
#include "device_identity.h"
#include "feed_request.h"
#include "fixture.h"
#include "regular_font.h"
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
  TEST_ASSERT_EQUAL_STRING("1'", countdown(a, 40000).c_str());
  TEST_ASSERT_EQUAL_STRING("0'", countdown(a, 40001).c_str());
  TEST_ASSERT_EQUAL_STRING("0'", countdown(a, 99999).c_str());
  TEST_ASSERT_TRUE(countdown(a, 100000).empty());
  a.provenance = Provenance::Scheduled;
  TEST_ASSERT_EQUAL_STRING("S1'", countdown(a, 40000).c_str());
  a.provenance = Provenance::Unknown;
  TEST_ASSERT_EQUAL_STRING("?0'", countdown(a, 99999).c_str());
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
    auto view = viewAt(snapshot, snapshot.feed.generatedAt, 5000, true);
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
      "S0'", countdown(view.stops[3].groups[0].arrivals[0], feed.generatedAt).c_str());
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
  TEST_ASSERT_EQUAL_STRING("/v1/device/feed", DeviceFeedPath);
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
void personalized_feed_states() {
  const auto response = readResponse(200, FixtureJson, "");
  TEST_ASSERT_TRUE(response.error == RequestError::None);
  const auto original = fixture();
  TEST_ASSERT_EQUAL(original.stops.size(), response.feed.stops.size());
  for (size_t i = 0; i < original.stops.size(); ++i)
    TEST_ASSERT_EQUAL_STRING(original.stops[i].id.c_str(), response.feed.stops[i].id.c_str());
  const auto empty = readResponse(200, R"({"version":1,"generatedAt":1000,"stops":[]})", "");
  TEST_ASSERT_TRUE(empty.error == RequestError::None);
  Snapshot snapshot = sample();
  Snapshot unconfigured;
  const auto missing = readResponse(409, "ignored", "");
  TEST_ASSERT_TRUE(missing.error == RequestError::Configuration);
  TEST_ASSERT_EQUAL(300000, missing.retryMs);
  unconfigured.error = missing.error;
  applySnapshot(snapshot, std::move(unconfigured));
  TEST_ASSERT_TRUE(snapshot.hasFeed);
  TEST_ASSERT_EQUAL_STRING("ADD STOPS", viewAt(snapshot, 1000, 1000, true).message.c_str());
  Snapshot cleared;
  cleared.hasFeed = true;
  cleared.feed = empty.feed;
  cleared.receivedAtMs = 1000;
  applySnapshot(snapshot, std::move(cleared));
  TEST_ASSERT_TRUE(snapshot.feed.stops.empty());
  TEST_ASSERT_EQUAL_STRING("ADD STOPS", viewAt(snapshot, 1000, 1000, true).message.c_str());
  TEST_ASSERT_EQUAL(300000, readResponse(401, "ignored", "").retryMs);
}
void persistent_identity_validation() {
  const std::string token = "dev1_" + std::string(64, 'a');
  const std::string valid =
      "{\"deviceId\":\"12345678-1234-1234-1234-123456789abc\",\"token\":\"" + token + "\"}";
  DeviceIdentity identity;
  TEST_ASSERT_TRUE(parseDeviceIdentity(valid, identity));
  TEST_ASSERT_EQUAL_STRING(token.c_str(), identity.token.c_str());
  for (const auto &bad : {std::string("{}"), std::string("null"), std::string(257, 'x'),
                          std::string("{\"deviceId\":\"bad\",\"token\":\"bad\"}")}) {
    TEST_ASSERT_FALSE(parseDeviceIdentity(bad, identity));
    TEST_ASSERT_EQUAL_STRING(token.c_str(), identity.token.c_str());
  }
  auto bad = valid;
  bad.replace(bad.find("dev1_"), 5, "oops_");
  TEST_ASSERT_FALSE(parseDeviceIdentity(bad, identity));
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
  TEST_ASSERT_EQUAL(4, renderDiagnostic(frame, Diagnostic::Red, 0));
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
  constexpr Color expected[] = {0xce1126, 0xce1126, 0x1eb53a, 0x1eb53a, 0xc62a0f, 0x2d4191};
  for (size_t page = 0; page < 6; ++page) {
    renderer.render(frame, view, snapshot.feed.generatedAt, page * PageDurationMs);
    TEST_ASSERT_EQUAL_HEX32(expected[page], frame.pixel(1, 11));
  }
  TEST_ASSERT_EQUAL_STRING("#C62A0F", snapshot.feed.stops[1].groups[0].lineColor.c_str());
  TEST_ASSERT_EQUAL_STRING("#FFFFFF", view.stops[1].groups[0].lineTextColor.c_str());
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
    doc["stops"][1]["groups"][0].remove("lineColor");
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
void badge_typography_comparisons() {
  using namespace badgecontrast;
  Framebuffer frame(64, 32), regular(64, 32);
  constexpr Comparison pairs[] = {Comparison::BlackWhite, Comparison::BlackBold,
                                  Comparison::WhiteBold};
  constexpr Face expected[][2] = {{Face::BlackRegular, Face::WhiteRegular},
                                  {Face::BlackRegular, Face::WhiteBold},
                                  {Face::WhiteRegular, Face::WhiteBold}};
  for (unsigned index = 0; index < Count; ++index) {
    for (unsigned pair = 0; pair < 3; ++pair) {
      render(regular, index);
      for (bool swapped : {false, true}) {
        render(frame, index, swapped, pairs[pair]);
        for (int side = 0; side < 2; ++side) {
          const Face face = expected[pair][swapped ? 1 - side : side];
          TEST_ASSERT_TRUE(faceFor(pairs[pair], side, swapped) == face);
          const int x = side ? 34 : 1;
          unsigned inkPixels = 0, regularPixels = 0;
          for (int y = 10; y < 21; ++y)
            for (int dx = 0; dx < 29; ++dx) {
              const auto pixel = frame.pixel(x + dx, y);
              const bool regularInk = regular.pixel(34 + dx, y) == 0xffffff;
              if (regularInk)
                ++regularPixels;
              if (face != Face::WhiteBold)
                TEST_ASSERT_EQUAL_HEX32(regularInk ? (face == Face::BlackRegular ? 0 : 0xffffff)
                                                   : Samples[index].background,
                                        pixel);
              TEST_ASSERT_TRUE(pixel == Samples[index].background ||
                               pixel == (face == Face::BlackRegular ? 0 : 0xffffff));
              if (pixel != Samples[index].background)
                ++inkPixels;
            }
          TEST_ASSERT_TRUE(inkPixels > 0);
          if (face == Face::WhiteBold) {
            TEST_ASSERT_TRUE(inkPixels > regularPixels);
            TEST_ASSERT_TRUE(boldWidth(Samples[index].label) <= 29);
          }
          for (int dx = 0; dx < 29; ++dx) {
            TEST_ASSERT_EQUAL_HEX32(Samples[index].background, frame.pixel(x + dx, 10));
            TEST_ASSERT_EQUAL_HEX32(Samples[index].background, frame.pixel(x + dx, 20));
          }
        }
        for (int y = 0; y < 32; ++y) {
          TEST_ASSERT_EQUAL(0, frame.pixel(0, y));
          TEST_ASSERT_EQUAL(0, frame.pixel(63, y));
          if (y >= 10 && y < 21)
            TEST_ASSERT_EQUAL(0, frame.pixel(31, y));
        }
      }
    }
  }
  // The bold zero retains a two-pixel-wide open counter rather than filling it in.
  frame.clear(0xfb712b);
  drawBold(frame, "0", 1, 12);
  for (int y = 13; y <= 17; ++y) {
    TEST_ASSERT_EQUAL_HEX32(0xfb712b, frame.pixel(3, y));
    TEST_ASSERT_EQUAL_HEX32(0xfb712b, frame.pixel(4, y));
    TEST_ASSERT_EQUAL_HEX32(0xffffff, frame.pixel(1, y));
    TEST_ASSERT_EQUAL_HEX32(0xffffff, frame.pixel(6, y));
  }
}
void pagination_without_indicators() {
  auto snapshot = sample();
  auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  for (auto &stop : view.stops)
    for (auto &group : stop.groups)
      group.destination = "FONDO";
  const auto pages = pagesFor(view, 32);
  TEST_ASSERT_EQUAL(6, pages.size());
  Framebuffer first(64, 32), next(64, 32), repeat(64, 32);
  ArrivalRenderer renderer;
  renderer.render(first, view, snapshot.feed.generatedAt, 0);
  renderer.render(next, view, snapshot.feed.generatedAt, PageDurationMs);
  renderer.render(repeat, view, snapshot.feed.generatedAt, pages.size() * PageDurationMs);
  for (int x = 0; x < 64; ++x) {
    TEST_ASSERT_EQUAL(0, first.pixel(x, 30));
    TEST_ASSERT_EQUAL(0, next.pixel(x, 30));
  }
  TEST_ASSERT_EQUAL_UINT64(pixelsHash(first), pixelsHash(repeat));
  TEST_ASSERT_NOT_EQUAL(pixelsHash(first), pixelsHash(next));
}
void destination_wrapping_and_geometry() {
  const auto hospital = destinationLines("Hospital de Bellvitge", 62);
  TEST_ASSERT_EQUAL(2, hospital.size());
  TEST_ASSERT_EQUAL_STRING("HOSPITAL DE", hospital[0].c_str());
  TEST_ASSERT_EQUAL_STRING("BELLVITGE", hospital[1].c_str());
  const auto zona = destinationLines("Zona Universitària", 62);
  TEST_ASSERT_EQUAL(2, zona.size());
  TEST_ASSERT_EQUAL_STRING("ZONA", zona[0].c_str());
  TEST_ASSERT_EQUAL_STRING("UNIVERSITARIA", zona[1].c_str());
  TEST_ASSERT_EQUAL(1, destinationLines("Trinitat Nova", 62).size());
  TEST_ASSERT_EQUAL(1, destinationLines("WWWWWWWWWWWWWWWWWWWWWW", 62).size());
  TEST_ASSERT_EQUAL(1, destinationLines("Zona Universitària", 60).size());
  auto snapshot = sample();
  auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  view.stops.resize(1);
  view.stops[0].groups.resize(1);
  ArrivalRenderer renderer;
  for (int width : {64, 128}) {
    const std::string label =
        width == 64 ? "Hospital de Bellvitge" : "Hospital de Bellvitge Hospital de Bellvitge";
    view.stops[0].groups[0].destination = label;
    const auto lines = destinationLines(label, width - 2);
    TEST_ASSERT_EQUAL(2, lines.size());
    Framebuffer frame(width, width / 2), later(width, width / 2), expected(width, width / 2);
    renderer.render(frame, view, snapshot.feed.generatedAt, 0);
    renderer.render(later, view, snapshot.feed.generatedAt, 1500);
    TEST_ASSERT_EQUAL_UINT64(pixelsHash(frame), pixelsHash(later));
    typography::drawC2(expected, lines[0], 1, 20, 0xe8eee8, 1, width - 1);
    typography::drawC2(expected, lines[1], 1, 26, 0xe8eee8, 1, width - 1);
    for (int y = 18; y < width / 2; ++y)
      for (int x = 0; x < width; ++x)
        TEST_ASSERT_EQUAL_HEX32(expected.pixel(x, y), frame.pixel(x, y));
    TEST_ASSERT_EQUAL_HEX32(0xe8eee8, frame.pixel(1, 20));
  }
  for (const char *label : {"FONDO", "WWWWWWWWWWWWWWWWWWWWWW"}) {
    view.stops[0].groups[0].destination = label;
    Framebuffer frame(64, 32), expected(64, 32);
    renderer.render(frame, view, snapshot.feed.generatedAt, 1500);
    typography::drawC2(expected, label, 1, 20, 0xe8eee8, 1, 63,
                       overflowOffset(typography::c2Width(label), 62, 1500));
    for (int y = 18; y < 32; ++y)
      for (int x = 0; x < 64; ++x)
        TEST_ASSERT_EQUAL_HEX32(expected.pixel(x, y), frame.pixel(x, y));
  }
}
void explicit_badge_foregrounds() {
  for (const char *ink : {"#000000", "#FFFFFF"}) {
    auto snapshot = sample();
    DynamicJsonDocument doc(128 * 1024);
    deserializeJson(doc, FixtureJson);
    doc["stops"][0]["mode"] = "opaque";
    doc["stops"][0]["groups"][0]["lineColor"] = "#CE1126";
    doc["stops"][0]["groups"][0]["lineTextColor"] = ink;
    std::string json;
    serializeJson(doc, json);
    TEST_ASSERT_TRUE(parseFeed(json, snapshot.feed) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_STRING(ink, snapshot.feed.stops[0].groups[0].lineTextColor.c_str());
    auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
    ArrivalRenderer renderer;
    Framebuffer frame(64, 32);
    renderer.render(frame, view, snapshot.feed.generatedAt, 0);
    // L's top-left stroke and badge padding: explicit ink, unchanged background.
    TEST_ASSERT_EQUAL_HEX32(ink[1] == '0' ? 0 : 0xffffff, frame.pixel(2, 12));
    TEST_ASSERT_EQUAL_HEX32(0xce1126, frame.pixel(1, 11));
  }
  for (const char *invalid : {"#fff", "red", "#12345z"}) {
    DynamicJsonDocument doc(128 * 1024);
    deserializeJson(doc, FixtureJson);
    doc["stops"][0]["groups"][0]["lineTextColor"] = invalid;
    std::string json;
    serializeJson(doc, json);
    Feed output;
    TEST_ASSERT_TRUE(parseFeed(json, output) == ParseResult::Invalid);
  }
  auto snapshot = sample();
  TEST_ASSERT_TRUE(parseFeed(StatesJson, snapshot.feed) == ParseResult::Ok);
  const auto &stop = snapshot.feed.stops[snapshot.feed.stops.size() - 2];
  constexpr Color colors[] = {0x547b31, 0x894796, 0x2d4191, 0x000000, 0xc62a0f};
  View view;
  view.stops.push_back(stop);
  Framebuffer frame(64, 32);
  ArrivalRenderer renderer;
  for (unsigned i = 0; i < 5; ++i) {
    renderer.render(frame, view, snapshot.feed.generatedAt, i * PageDurationMs);
    TEST_ASSERT_EQUAL_HEX32(colors[i], frame.pixel(1, 11));
    bool whiteInk = false;
    for (int y = 12; y < 17; ++y)
      for (int x = 2; x < 15; ++x)
        whiteInk = whiteInk || frame.pixel(x, y) == 0xffffff;
    TEST_ASSERT_TRUE(whiteInk);
  }
  // Every verified Metro policy fixture uses its explicit foreground, independent of label.
  view.stops[0] = snapshot.feed.stops[snapshot.feed.stops.size() - 3];
  for (size_t i = 0; i < view.stops[0].groups.size(); ++i) {
    const auto &group = view.stops[0].groups[i];
    renderer.render(frame, view, snapshot.feed.generatedAt, i * PageDurationMs);
    TEST_ASSERT_EQUAL_HEX32(group.lineTextColor == "#000000" ? 0 : 0xffffff, frame.pixel(2, 12));
  }
  // A legacy colourless bus keeps its existing green fallback without inferring V family.
  view.stops[0] = snapshot.feed.stops.back();
  renderer.render(frame, view, snapshot.feed.generatedAt, 0);
  TEST_ASSERT_EQUAL_HEX32(0x42ac52, frame.pixel(1, 11));
}
void bus_stop_badge_is_independent_of_line_colors() {
  auto snapshot = sample();
  TEST_ASSERT_TRUE(parseFeed(StatesJson, snapshot.feed) == ParseResult::Ok);
  View view;
  view.stops.push_back(snapshot.feed.stops[snapshot.feed.stops.size() - 2]);
  Framebuffer frame(64, 32);
  ArrivalRenderer renderer;
  constexpr Color lineColors[] = {0x547b31, 0x894796, 0x2d4191, 0x000000, 0xc62a0f};
  for (unsigned page = 0; page < 5; ++page) {
    renderer.render(frame, view, snapshot.feed.generatedAt, page * PageDurationMs);
    TEST_ASSERT_EQUAL_HEX32(0xc62a0f, frame.pixel(1, 1));
    TEST_ASSERT_EQUAL_HEX32(0xffffff, frame.pixel(2, 2));
    TEST_ASSERT_EQUAL_HEX32(lineColors[page], frame.pixel(1, 11));
  }
  // Explicit backend foreground still wins on a line, independently of header ink.
  view.stops[0].groups[0].lineTextColor = "#000000";
  renderer.render(frame, view, snapshot.feed.generatedAt, 0);
  TEST_ASSERT_EQUAL_HEX32(0, frame.pixel(2, 12));
  TEST_ASSERT_EQUAL_HEX32(0xffffff, frame.pixel(2, 2));
}
void operational_badge_uses_regular_x() {
  auto snapshot = sample();
  auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  view.stops.front().groups.front().line = "X";
  view.stops.front().groups.front().lineColor = "#123456";
  view.stops.front().groups.front().lineTextColor = "#FFFFFF";
  Framebuffer frame(64, 32);
  ArrivalRenderer renderer;
  renderer.render(frame, view, snapshot.feed.generatedAt, 0);
  const uint8_t rows[] = {9, 9, 6, 9, 9};
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 4; ++x)
      TEST_ASSERT_EQUAL_HEX32(rows[y] & (1 << (3 - x)) ? 0xffffff : 0x123456,
                              frame.pixel(2 + x, 12 + y));
}
void brightness_presets_are_bounded() {
  uint8_t value = 16;
  for (const auto preset : {4, 8, 16, 32}) {
    TEST_ASSERT_TRUE(brightnessPreset("b" + std::to_string(preset), value));
    TEST_ASSERT_EQUAL(preset, value);
  }
  for (const auto *invalid : {"b0", "b255", "b-1", "b16extra", "", "16"}) {
    TEST_ASSERT_FALSE(brightnessPreset(invalid, value));
    TEST_ASSERT_EQUAL(32, value);
  }
}
void v23_contrast_uses_operational_c2() {
  constexpr Color inks[] = {0xd9d9d9, 0xd9d9d9, 0xd9d9d9, 0xd9d9d9};
  constexpr Color backgrounds[] = {0x385322, 0x286b12, 0x207a10, 0x185c08};
  for (unsigned variant = 0; variant < 4; ++variant) {
    Framebuffer frame(64, 32);
    badgecontrast::renderV23(frame, variant);
    TEST_ASSERT_EQUAL_HEX32(backgrounds[variant], frame.pixel(1, 11));
    Framebuffer expected(64, 32);
    typography::drawC2(expected, "V23", 2, 12, 1, 1, 63);
    for (int y = 12; y < 17; ++y)
      for (int x = 2; x < 17; ++x)
        if (expected.pixel(x, y))
          TEST_ASSERT_EQUAL_HEX32(inks[variant], frame.pixel(x, y));
        else
          TEST_ASSERT_EQUAL_HEX32(backgrounds[variant], frame.pixel(x, y));
  }
}
void reviewed_raster_regression() {
  auto snapshot = sample();
  auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  ArrivalRenderer renderer;
  Framebuffer frame(64, 32);
  // Keep the reviewed neutral-colour geometry baseline; separate tests cover new colours.
  for (auto &stop : view.stops)
    for (auto &group : stop.groups) {
      group.lineColor = "#777777";
      group.lineTextColor.clear();
    }
  // Active C2 typography raster baselines; exact glyph masks are checked separately.
  renderer.render(frame, view, snapshot.feed.generatedAt, 0);
  TEST_ASSERT_EQUAL_UINT64(16407060266703154628ULL, pixelsHash(frame));
  renderer.render(frame, view, snapshot.feed.generatedAt, 5 * PageDurationMs);
  TEST_ASSERT_EQUAL_UINT64(14666369046108617792ULL, pixelsHash(frame));
  // A header background-only substitution keeps text and geometry fixed.
  auto legacyHeader = frame;
  for (int y = 1; y < 8; ++y)
    for (int x = 1; x < 8; ++x)
      if (legacyHeader.pixel(x, y) == 0xc62a0f)
        legacyHeader.pixel(x, y, 0x42ac52);
  TEST_ASSERT_EQUAL_UINT64(16916934630398909737ULL, pixelsHash(legacyHeader));
  TEST_ASSERT_TRUE(parseFeed(StatesJson, snapshot.feed) == ParseResult::Ok);
  view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  renderer.render(frame, view, snapshot.feed.generatedAt, 2 * PageDurationMs);
  TEST_ASSERT_EQUAL_UINT64(8114341266584107742ULL, pixelsHash(frame));
  renderer.render(frame, view, snapshot.feed.generatedAt, 3 * PageDurationMs);
  TEST_ASSERT_EQUAL_UINT64(18245193702606040162ULL, pixelsHash(frame));
}
} // namespace
void interruption_retains_feed_and_recovers() {
  for (auto error : {RequestError::Network, RequestError::Http, RequestError::InvalidFeed,
                     RequestError::RateLimited}) {
    auto snapshot = sample();
    const auto generatedAt = snapshot.feed.generatedAt;
    Snapshot failed;
    failed.error = error;
    failed.receivedAtMs = 90000;
    applySnapshot(snapshot, std::move(failed));
    TEST_ASSERT_TRUE(snapshot.hasFeed);
    TEST_ASSERT_EQUAL(1000, snapshot.receivedAtMs);
    TEST_ASSERT_EQUAL_INT64(generatedAt, snapshot.feed.generatedAt);
    const auto warning = viewAt(snapshot, generatedAt + 4000, 5000, true);
    TEST_ASSERT_TRUE(warning.stops.empty());
    TEST_ASSERT_EQUAL_STRING(requestMessage(error), warning.message.c_str());
    const auto retained = viewAt(snapshot, generatedAt + 5000, 6000, true);
    TEST_ASSERT_EQUAL(2, retained.stops.size());
    TEST_ASSERT_EQUAL(4, retained.stops[0].groups.size());
    TEST_ASSERT_EQUAL(2, retained.stops[1].groups.size());
    const auto expired = viewAt(snapshot, generatedAt + 120000, 121000, true);
    TEST_ASSERT_TRUE(expired.stops[0].status == StopStatus::Stale);
    TEST_ASSERT_TRUE(expired.stops[0].groups.empty());
    auto recovered = sample();
    recovered.feed.generatedAt += 120000;
    recovered.receivedAtMs = 121000;
    applySnapshot(snapshot, std::move(recovered));
    const auto healthy = viewAt(snapshot, generatedAt + 120000, 121000, true);
    TEST_ASSERT_TRUE(snapshot.error == RequestError::None);
    TEST_ASSERT_TRUE(healthy.message.empty());
    TEST_ASSERT_TRUE(healthy.stops[0].status == StopStatus::Ok);
  }
}
void interruption_still_expires_predictions() {
  auto snapshot = sample();
  snapshot.feed.stops[0].groups[0].arrivals[0].arrivalAt = snapshot.feed.generatedAt + 2000;
  Snapshot failed;
  failed.error = RequestError::Network;
  applySnapshot(snapshot, std::move(failed));
  const auto view = viewAt(snapshot, snapshot.feed.generatedAt + 5000, 6000, true);
  TEST_ASSERT_EQUAL(1, view.stops[0].groups[0].arrivals.size());
  TEST_ASSERT_EQUAL(2, snapshot.feed.stops[0].groups[0].arrivals.size());
  TEST_ASSERT_EQUAL_STRING("CLOCK", viewAt(snapshot, 0, 6000, false).message.c_str());
}
void three_prediction_rows() {
  Group group;
  auto set = [&](std::initializer_list<int> minutes) {
    group.arrivals.clear();
    for (int minute : minutes)
      group.arrivals.push_back({1001 + minute * 60000LL, Provenance::Realtime, "", ""});
  };
  set({0});
  TEST_ASSERT_EQUAL_STRING("0'", arrivalText(group, 1000, 49).c_str());
  set({2, 8});
  TEST_ASSERT_EQUAL_STRING("2' 8'", arrivalText(group, 1000, 49).c_str());
  set({0, 10, 19, 24});
  TEST_ASSERT_EQUAL_STRING("0' 10' 19'", arrivalText(group, 1000, 49).c_str());
  TEST_ASSERT_EQUAL_STRING("9' 18' 23'", arrivalText(group, 61001, 49).c_str());
  set({12, 24, 38});
  TEST_ASSERT_EQUAL_STRING("12' 24' 38'", arrivalText(group, 1000, 49).c_str());
  TEST_ASSERT_EQUAL_STRING("12' 24'", arrivalText(group, 1000, 30).c_str());
  TEST_ASSERT_TRUE(arrivalText(group, 1000, 10).empty());
  set({99, 99, 99});
  TEST_ASSERT_EQUAL_STRING("99' 99' 99'", arrivalText(group, 1000, 44).c_str());
  TEST_ASSERT_EQUAL_STRING("99' 99'", arrivalText(group, 1000, 43).c_str());
}
void three_prediction_framebuffer() {
  auto snapshot = sample();
  auto view = viewAt(snapshot, snapshot.feed.generatedAt, 1000, true);
  view.stops.resize(1);
  view.stops[0].groups.resize(1);
  auto &group = view.stops[0].groups[0];
  group.destination = "FONDO";
  ArrivalRenderer renderer;
  const std::vector<std::vector<int>> cases = {{0, 10, 19}, {2, 8, 15}, {12, 24, 38}};
  const char *labels[] = {"0' 10' 19'", "2' 8' 15'", "12' 24' 38'"};
  for (const char *line : {"L3", "LONG LINE"}) {
    group.line = line;
    const int badgeWidth = std::min(30, typography::c2Width(line) + 2);
    const int slotLeft = 1 + badgeWidth + 2;
    for (size_t i = 0; i < cases.size(); ++i) {
      group.arrivals.clear();
      Framebuffer baseline(64, 32), actual(64, 32), expected(64, 32);
      renderer.render(baseline, view, 1000, 0);
      for (int minute : cases[i])
        group.arrivals.push_back({1001 + minute * 60000LL, Provenance::Realtime, "", ""});
      const std::string label = badgeWidth == 30 ? (i == 0   ? "0' 10'"
                                                    : i == 1 ? "2' 8'"
                                                             : "12' 24'")
                                                 : labels[i];
      TEST_ASSERT_EQUAL_STRING(label.c_str(), arrivalText(group, 1000, 63 - slotLeft).c_str());
      const int width = typography::c2Width(label);
      TEST_ASSERT_TRUE(63 - width >= slotLeft);
      // Draw the expected glyphs without clipping: missing minute-mark pixels or
      // a clipped right edge in the actual renderer must fail this comparison.
      typography::drawC2(expected, label, 63 - width, 12, 0xaaff66, 0, 64);
      renderer.render(actual, view, 1000, 0);
      for (int y = 11; y <= 17; ++y) {
        for (int x = 0; x < slotLeft; ++x)
          TEST_ASSERT_EQUAL_HEX32(baseline.pixel(x, y), actual.pixel(x, y));
        for (int x = slotLeft; x < 64; ++x)
          TEST_ASSERT_EQUAL_HEX32(expected.pixel(x, y), actual.pixel(x, y));
      }
    }
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(interruption_retains_feed_and_recovers);
  RUN_TEST(interruption_still_expires_predictions);
  RUN_TEST(protocol_fixture);
  RUN_TEST(forward_compatibility);
  RUN_TEST(legacy_fields);
  RUN_TEST(reject_versions_transactionally);
  RUN_TEST(reject_bad_fields);
  RUN_TEST(bounded_resources);
  RUN_TEST(preserve_identifiers_and_color);
  RUN_TEST(countdown_boundaries);
  RUN_TEST(three_prediction_rows);
  RUN_TEST(three_prediction_framebuffer);
  RUN_TEST(freshness_boundary_and_monotonic_guard);
  RUN_TEST(clock_and_request_errors_suppress_arrivals);
  RUN_TEST(partial_scheduled_and_expiry);
  RUN_TEST(expiry_is_local_and_does_not_mutate_feed);
  RUN_TEST(request_path_and_credentials);
  RUN_TEST(http_failures_and_backoff);
  RUN_TEST(personalized_feed_states);
  RUN_TEST(persistent_identity_validation);
  RUN_TEST(pagination_capacity_and_order);
  RUN_TEST(label_reveal_and_utf8);
  RUN_TEST(raster_determinism_and_paging);
  RUN_TEST(diagnostics_and_clipping);
  RUN_TEST(fixture_line_colors);
  RUN_TEST(explicit_color_precedes_fallback);
  RUN_TEST(missing_color_stays_neutral);
  RUN_TEST(framebuffer_preserves_rgb);
  RUN_TEST(badge_contrast_pairs);
  RUN_TEST(badge_typography_comparisons);
  RUN_TEST(pagination_without_indicators);
  RUN_TEST(destination_wrapping_and_geometry);
  RUN_TEST(explicit_badge_foregrounds);
  RUN_TEST(bus_stop_badge_is_independent_of_line_colors);
  RUN_TEST(operational_badge_uses_regular_x);
  RUN_TEST(brightness_presets_are_bounded);
  RUN_TEST(v23_contrast_uses_operational_c2);
  RUN_TEST(reviewed_raster_regression);
  return UNITY_END();
}
