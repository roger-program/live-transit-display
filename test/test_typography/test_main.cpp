#include "feed_parser.h"
#include "fixture.h"
#include "typography_diagnostic.h"
#include <unity.h>

using namespace transit;
using typography::Face;
void setUp() {}
void tearDown() {}
namespace {
bool same(const Framebuffer &a, const Framebuffer &b) {
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x)
      if (a.pixel(x, y) != b.pixel(x, y))
        return false;
  return true;
}
void sample_palette_matches_existing_fixture() {
  Feed feed;
  TEST_ASSERT_TRUE(parseFeed(StatesJson, feed) == ParseResult::Ok);
  for (const auto &sample : typography::Samples) {
    bool found = false;
    // Existing fixture uses V31 for the V-family swatch; diagnostic uses V23.
    const std::string line = std::string(sample.line) == "V23" ? "V31" : sample.line;
    for (const auto &stop : feed.stops)
      for (const auto &group : stop.groups)
        if (group.line == line && !group.lineColor.empty()) {
          TEST_ASSERT_EQUAL_STRING(group.lineColor.c_str(), sample.background);
          TEST_ASSERT_EQUAL_STRING(group.lineTextColor.c_str(), sample.foreground);
          found = true;
        }
    TEST_ASSERT_TRUE(found);
  }
}
uint64_t rasterHash(const Framebuffer &frame) {
  uint64_t hash = 14695981039346656037ULL;
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x) {
      hash ^= frame.pixel(x, y);
      hash *= 1099511628211ULL;
    }
  return hash;
}
void original_c_rasters_are_unchanged() {
  // Captured before C2 changes: six original samples, four reveal phases each.
  constexpr uint64_t expected[6][4] = {
      {
          6313334295921434406ULL,
          1555733161246437846ULL,
          1555733161246437846ULL,
          1555733161246437846ULL,
      },
      {
          14456415335612399286ULL,
          6536940450219568726ULL,
          12870228134835537798ULL,
          6536940450219568726ULL,
      },
      {
          3929238620019417560ULL,
          3929238620019417560ULL,
          3929238620019417560ULL,
          3929238620019417560ULL,
      },
      {
          17260872549701333587ULL,
          6733157038434577587ULL,
          1281795804916322731ULL,
          6733157038434577587ULL,
      },
      {
          15948530755033704665ULL,
          18285982664928063097ULL,
          13042077144263896297ULL,
          18285982664928063097ULL,
      },
      {
          15477338888834312075ULL,
          4535416234175118923ULL,
          13715089068438336971ULL,
          4535416234175118923ULL,
      },
  };
  Framebuffer frame(64, 32);
  for (unsigned sample = 0; sample < 6; ++sample)
    for (unsigned step = 0; step < 4; ++step) {
      typography::render(frame, Face::C, sample, typography::Selection::Phases[step]);
      TEST_ASSERT_EQUAL_UINT64(expected[sample][step], rasterHash(frame));
    }
}
void c2_dimensions_counters_and_subset() {
  TEST_ASSERT_EQUAL(13, sizeof(typography::LabelsC2) / sizeof(typography::LabelsC2[0]));
  for (const auto &glyph : typography::LabelsC2) {
    const int columns = typography::c2Columns(glyph.character);
    TEST_ASSERT_EQUAL(glyph.character == 'I' ? 3 : 4, columns);
    bool left = false, right = false;
    for (int row = 0; row < 5; ++row) {
      TEST_ASSERT_GREATER_THAN(0, glyph.rows[row]);
      TEST_ASSERT_LESS_THAN(1 << columns, glyph.rows[row]);
      left = left || (glyph.rows[row] & (1 << (columns - 1)));
      right = right || (glyph.rows[row] & 1);
      if (row < 4)
        for (int col = 0; col < columns - 1; ++col) {
          const int pair = 3 << col;
          TEST_ASSERT_FALSE((glyph.rows[row] & pair) == pair &&
                            (glyph.rows[row + 1] & pair) == pair);
        }
    }
    TEST_ASSERT_TRUE(left && right);
  }
  const auto &o = typography::c2Glyph('O');
  for (int row = 1; row < 4; ++row)
    TEST_ASSERT_EQUAL(9, o.rows[row]);                    // Two open columns.
  TEST_ASSERT_EQUAL(7, typography::c2Glyph('I').rows[0]); // Barred I, not a stick.
  for (unsigned sample = 6; sample < typography::SampleCount; ++sample) {
    TEST_ASSERT_TRUE(typography::c2Supports(typography::Samples[sample].stop));
    TEST_ASSERT_TRUE(typography::c2Supports(typography::Samples[sample].destination));
  }
  TEST_ASSERT_FALSE(typography::c2Supports("Bus 23"));
  TEST_ASSERT_FALSE(typography::c2Supports("🚇"));
  Framebuffer actual(64, 32), fallback(64, 32);
  typography::draw(actual, Face::C2, "B", 0, 0, 1, 0, 64);
  typography::draw(fallback, Face::C2, "?", 0, 0, 1, 0, 64);
  TEST_ASSERT_TRUE(same(actual, fallback));
}
void c2_measured_width_clipping_and_reveal() {
  const char *words[] = {"LESSEPS", "LA PAU", "TRINITAT NOVA", "ZONA UNIVERSITARIA"};
  const int widths[] = {34, 27, 60, 84};
  for (unsigned sample = 0; sample < 4; ++sample) {
    Framebuffer frame(128, 32);
    typography::draw(frame, Face::C2, words[sample], 0, 0, 1, 0, 128);
    int last = 0;
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 128; ++x)
        if (frame.pixel(x, y))
          last = std::max(last, x + 1);
    TEST_ASSERT_EQUAL(widths[sample], last);
    TEST_ASSERT_EQUAL(last, typography::width(Face::C2, words[sample]));
  }
  TEST_ASSERT_EQUAL(0, typography::width(Face::C2, "   "));
  TEST_ASSERT_EQUAL(4, typography::width(Face::C2, "A "));
  TEST_ASSERT_EQUAL(7, typography::width(Face::C2, " A"));
  TEST_ASSERT_EQUAL(84, typography::width(Face::C2, "Zona Universitària"));
  Framebuffer frame(64, 32);
  const int offset = overflowOffset(typography::width(Face::C2, words[3]), 61, 1600);
  TEST_ASSERT_EQUAL(23, offset);
  typography::draw(frame, Face::C2, words[3], 2, 21, 1, 2, 63, offset);
  bool finalColumn = false;
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x) {
      if (x < 2 || x >= 63 || y < 21 || y >= 26)
        TEST_ASSERT_EQUAL(0, frame.pixel(x, y));
      if (x == 62 && frame.pixel(x, y))
        finalColumn = true;
    }
  TEST_ASSERT_TRUE(finalColumn);
}
void focused_countdowns_are_identical() {
  Framebuffer baseline(64, 32), candidate(64, 32);
  for (unsigned sample = 6; sample < typography::SampleCount; ++sample)
    for (auto phase : typography::Selection::Phases) {
      typography::render(baseline, Face::A, sample, phase);
      for (Face face : {Face::C, Face::C2}) {
        typography::render(candidate, face, sample, phase);
        for (int y = 12; y < 17; ++y)
          for (int x = 0; x < 64; ++x)
            TEST_ASSERT_EQUAL_HEX32(baseline.pixel(x, y), candidate.pixel(x, y));
      }
    }
}
void operational_baseline_is_exact() {
  ArrivalRenderer renderer;
  Framebuffer actual(64, 32), expected(64, 32);
  for (unsigned sample = 0; sample < typography::SampleCount; ++sample)
    for (auto phase : typography::Selection::Phases) {
      renderer.render(expected, typography::sampleView(sample), typography::Now, phase);
      typography::render(actual, Face::A, sample, phase);
      TEST_ASSERT_TRUE(same(actual, expected));
    }
}
void only_text_slots_change_and_badges_are_regular() {
  Framebuffer baseline(64, 32), alternative(64, 32);
  for (unsigned sample = 0; sample < typography::SampleCount; ++sample)
    for (auto phase : typography::Selection::Phases) {
      typography::render(baseline, Face::A, sample, phase);
      const auto &input = typography::Samples[sample];
      const int slotLeft = 1 + std::min(30, textWidth(input.line) + 2) + 2;
      for (Face face : {Face::B, Face::C, Face::D, Face::C2}) {
        typography::render(alternative, face, sample, phase);
        TEST_ASSERT_FALSE(same(baseline, alternative));
        for (int y = 0; y < 32; ++y)
          for (int x = 0; x < 64; ++x) {
            const bool label = y >= 2 && y < 7 && x >= 10 && x < 63;
            const bool destination = y >= 21 && y < 26 && x >= 2 && x < 63;
            const bool arrivals = y >= 12 && y < 17 && x >= slotLeft && x < 63;
            if (!label && !destination && !arrivals)
              TEST_ASSERT_EQUAL_HEX32(baseline.pixel(x, y), alternative.pixel(x, y));
          }
      }
    }
}
void exact_browser_shapes_and_spacing() {
  Framebuffer frame(64, 32);
  typography::draw(frame, Face::B, "A", 0, 0, 0xffffff, 0, 64);
  constexpr int b[] = {2, 5, 5, 7, 5};
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 3; ++x)
      TEST_ASSERT_EQUAL_HEX32(b[y] & (1 << (2 - x)) ? 0xffffff : 0, frame.pixel(x, y));
  frame.clear();
  typography::draw(frame, Face::C, "2M 9M", 0, 0, 0xffffff, 0, 64, 0, true);
  TEST_ASSERT_EQUAL_HEX32(0xffffff, frame.pixel(4, 0));  // Full five-column C 2.
  TEST_ASSERT_EQUAL_HEX32(0xffffff, frame.pixel(13, 0)); // Compact second 9.
  TEST_ASSERT_EQUAL(18, typography::width(Face::C, "2M 9M", true));
  TEST_ASSERT_EQUAL(27, typography::width(Face::B, "Lesseps"));
  TEST_ASSERT_EQUAL(21, typography::width(Face::D, "Lesseps"));
  TEST_ASSERT_EQUAL(62, typography::width(Face::B, "Zona Universitària"));
  TEST_ASSERT_EQUAL(60, typography::width(Face::D, "Zona Universitària"));
  TEST_ASSERT_NOT_EQUAL(typography::LabelsB[0].rows[0], typography::LabelsC[0].rows[0]);
}
void normalization_clipping_and_reveal() {
  Framebuffer actual(64, 32), expected(64, 32);
  for (Face face : {Face::B, Face::C, Face::D}) {
    typography::draw(actual, face, "Gràcia Ç ü", -10, 21, 0xe8eee8, 2, 20);
    typography::draw(expected, face, "GRACIA C U", -10, 21, 0xe8eee8, 2, 20);
    TEST_ASSERT_TRUE(same(actual, expected));
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 64; ++x)
        if (x < 2 || x >= 20 || y < 21 || y >= 26)
          TEST_ASSERT_EQUAL_HEX32(0, actual.pixel(x, y));
    actual.clear();
    expected.clear();
    typography::draw(actual, face, "🚇", 0, 0, 1, 0, 64);
    typography::draw(expected, face, "?", 0, 0, 1, 0, 64);
    TEST_ASSERT_TRUE(same(actual, expected));
    actual.clear();
    expected.clear();
    // Terminal phase exposes the last glyph, including the final safe column.
    const std::string longName = "Parc de la Vall d'Hebron";
    const int offset = overflowOffset(typography::width(face, longName), 61, 1600);
    TEST_ASSERT_GREATER_THAN(0, offset);
    typography::draw(actual, face, longName, 2, 21, 1, 2, 63, offset);
    bool lastColumn = false;
    for (int y = 21; y < 26; ++y)
      lastColumn = lastColumn || actual.pixel(62, y) == 1;
    TEST_ASSERT_TRUE(lastColumn);
    actual.clear();
  }
  TEST_ASSERT_EQUAL_STRING("D?HEBRON", pixelText("d’Hebron").c_str());
  TEST_ASSERT_EQUAL_STRING("A-B", pixelText("a-b").c_str());
  TEST_ASSERT_EQUAL_STRING("A?B", pixelText("a–b").c_str());
}
void serial_selection_is_repeatable() {
  typography::Selection state;
  TEST_ASSERT_TRUE(state.face == Face::A);
  TEST_ASSERT_EQUAL(0, state.phase());
  const unsigned initialSample = state.sample;
  TEST_ASSERT_TRUE(state.command('p'));
  TEST_ASSERT_EQUAL((initialSample + typography::SampleCount - 1) % typography::SampleCount,
                    state.sample);
  TEST_ASSERT_TRUE(state.command('n'));
  TEST_ASSERT_EQUAL(initialSample, state.sample);
  state.command('s');
  state.command('s');
  state.command('c');
  TEST_ASSERT_TRUE(state.face == Face::C);
  TEST_ASSERT_EQUAL(1600, state.phase());
  state.command('d');
  state.command('5');
  TEST_ASSERT_TRUE(state.face == Face::D);
  TEST_ASSERT_EQUAL(5, state.sample);
  TEST_ASSERT_FALSE(state.command('\n'));
  TEST_ASSERT_TRUE(state.command('9'));
  state.command('e');
  TEST_ASSERT_TRUE(state.face == Face::C2);
  TEST_ASSERT_EQUAL(9, state.sample);
  TEST_ASSERT_EQUAL(1600, state.phase());
  state.command('r');
  TEST_ASSERT_EQUAL(0, state.phase());
}
} // namespace
int main() {
  UNITY_BEGIN();
  RUN_TEST(sample_palette_matches_existing_fixture);
  RUN_TEST(original_c_rasters_are_unchanged);
  RUN_TEST(c2_dimensions_counters_and_subset);
  RUN_TEST(c2_measured_width_clipping_and_reveal);
  RUN_TEST(focused_countdowns_are_identical);
  RUN_TEST(operational_baseline_is_exact);
  RUN_TEST(only_text_slots_change_and_badges_are_regular);
  RUN_TEST(exact_browser_shapes_and_spacing);
  RUN_TEST(normalization_clipping_and_reveal);
  RUN_TEST(serial_selection_is_repeatable);
  return UNITY_END();
}
