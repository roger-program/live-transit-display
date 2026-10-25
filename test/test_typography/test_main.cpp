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
void original_c_labels_with_regular_badges() {
  // Captured before C2 changes: six original samples, four reveal phases each.
  constexpr uint64_t expected[6][4] = {
      {
          7749878141983216069ULL,
          9931406020408383525ULL,
          9931406020408383525ULL,
          9931406020408383525ULL,
      },
      {
          13422728455129996514ULL,
          3449185746832397186ULL,
          14918542989308745010ULL,
          3449185746832397186ULL,
      },
      {
          16896965383166995204ULL,
          16896965383166995204ULL,
          16896965383166995204ULL,
          16896965383166995204ULL,
      },
      {
          14243549861090742104ULL,
          17324614225099239448ULL,
          17564728952860026400ULL,
          17324614225099239448ULL,
      },
      {
          4342782278554189309ULL,
          5829578235396363885ULL,
          9357593157961210909ULL,
          5829578235396363885ULL,
      },
      {
          12858854701760518983ULL,
          8920544667613570791ULL,
          8771529488988145159ULL,
          8920544667613570791ULL,
      },
  };
  Framebuffer frame(64, 32);
  for (unsigned sample = 0; sample < 6; ++sample)
    for (unsigned step = 0; step < 4; ++step) {
      typography::render(frame, Face::C, sample, typography::Selection::Phases[step]);
      TEST_ASSERT_EQUAL_UINT64(expected[sample][step], rasterHash(frame));
    }
}
void comparison_faces_with_regular_badges() {
  // Comparison-label rasters now share active C2 badges; individual B/C/D masters remain unchanged.
  constexpr uint64_t expected[] = {
      13951603525253931717ULL,
      17005051676223190861ULL,
      9353075426315248013ULL,
      15100612861453528333ULL,
  };
  Framebuffer frame(64, 32);
  for (unsigned face = 0; face < 4; ++face) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned sample = 0; sample < 17; ++sample)
      for (auto phase : typography::Selection::Phases) {
        typography::render(frame, static_cast<Face>(face), sample, phase);
        hash ^= rasterHash(frame);
        hash *= 1099511628211ULL;
      }
    TEST_ASSERT_EQUAL_UINT64(expected[face], hash);
  }
}
void established_c2_glyphs_are_unchanged() {
  constexpr transit::Glyph OriginalC2[] = {
      {'A', {6, 9, 15, 9, 9}},  {'E', {15, 8, 14, 8, 15}}, {'I', {7, 2, 2, 2, 7}},
      {'L', {8, 8, 8, 8, 15}},  {'N', {9, 13, 11, 9, 9}},  {'O', {15, 9, 9, 9, 15}},
      {'P', {15, 9, 15, 8, 8}}, {'R', {15, 9, 15, 10, 9}}, {'S', {15, 8, 15, 1, 15}},
      {'T', {15, 4, 4, 4, 4}},  {'U', {9, 9, 9, 9, 15}},   {'V', {9, 9, 9, 10, 4}},
      {'Z', {15, 1, 6, 8, 15}},
  };

  for (const auto &original : OriginalC2) {
    const auto &current = typography::c2Glyph(original.character);
    TEST_ASSERT_EQUAL(original.character, current.character);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original.rows, current.rows, 5);
    TEST_ASSERT_EQUAL(original.character == 'I' ? 3 : 4, typography::c2Columns(original.character));
  }
}
void complete_alphabet_and_remaining_character_audit() {
  const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  TEST_ASSERT_TRUE(typography::c2Supports(alphabet));
  TEST_ASSERT_TRUE(typography::c2Supports("abcdefghijklmnopqrstuvwxyz"));
  TEST_ASSERT_TRUE(typography::c2Missing(alphabet).empty());
  for (char character : alphabet) {
    TEST_ASSERT_EQUAL(character, typography::c2Glyph(character).character);
    for (char other : alphabet)
      if (character != other) {
        const auto &glyph = typography::c2Glyph(character);
        const auto &alternative = typography::c2Glyph(other);
        TEST_ASSERT_FALSE(typography::c2Columns(character) == typography::c2Columns(other) &&
                          std::equal(glyph.rows, glyph.rows + 5, alternative.rows));
      }
  }
  TEST_ASSERT_TRUE(typography::c2Missing("012345678901").empty());
  TEST_ASSERT_EQUAL_STRING(",:!+?", typography::c2Missing("-.'/,:!+?").c_str());
  TEST_ASSERT_EQUAL_STRING("?", typography::c2Missing("🚇—").c_str());
  TEST_ASSERT_TRUE(typography::c2Supports("Gràcia Ç ü Ñ"));
  std::string covered;
  for (unsigned sample = typography::AlphabetSample; sample < typography::DestinationSample;
       ++sample) {
    const auto text = typography::Samples[sample].destination;
    TEST_ASSERT_TRUE(typography::c2Supports(text));
    TEST_ASSERT_TRUE(typography::width(Face::C2, text) <= 61);
    covered += text;
  }
  TEST_ASSERT_EQUAL_STRING(alphabet.c_str(), covered.c_str());
  for (const auto &text : {alphabet, std::string("MW IM"), std::string("Badalona Pompeu Fabra")}) {
    Framebuffer raster(256, 32);
    typography::draw(raster, Face::C2, text, 0, 0, 1, 0, 256);
    int inkRight = 0;
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 256; ++x)
        if (raster.pixel(x, y))
          inkRight = std::max(inkRight, x + 1);
    TEST_ASSERT_EQUAL(typography::width(Face::C2, text), inkRight);
  }
  TEST_ASSERT_EQUAL(11, typography::width(Face::C2, "MW"));
  Framebuffer arrivals(64, 32), operational(64, 32);
  typography::draw(arrivals, Face::C2, "0123456789", 0, 0, 1, 0, 64, 0, true);
  drawText(operational, "0123456789", 0, 0, 1, 0, 64);
  TEST_ASSERT_TRUE(same(arrivals, operational));
}
void validated_capitals_except_q_and_x_are_unchanged() {
  constexpr transit::Glyph before[] = {
      {'A', {6, 9, 15, 9, 9}},     {'B', {14, 9, 14, 9, 14}},   {'C', {15, 8, 8, 8, 15}},
      {'D', {14, 9, 9, 9, 14}},    {'E', {15, 8, 14, 8, 15}},   {'F', {15, 8, 14, 8, 8}},
      {'G', {15, 8, 11, 9, 15}},   {'H', {9, 9, 15, 9, 9}},     {'I', {7, 2, 2, 2, 7}},
      {'J', {7, 1, 1, 9, 15}},     {'K', {9, 10, 12, 10, 9}},   {'L', {8, 8, 8, 8, 15}},
      {'M', {17, 27, 21, 17, 17}}, {'N', {9, 13, 11, 9, 9}},    {'O', {15, 9, 9, 9, 15}},
      {'P', {15, 9, 15, 8, 8}},    {'Q', {15, 9, 9, 11, 1}},    {'R', {15, 9, 15, 10, 9}},
      {'S', {15, 8, 15, 1, 15}},   {'T', {15, 4, 4, 4, 4}},     {'U', {9, 9, 9, 9, 15}},
      {'V', {9, 9, 9, 10, 4}},     {'W', {17, 17, 21, 27, 17}}, {'X', {9, 6, 4, 6, 9}},
      {'Y', {9, 9, 6, 4, 4}},      {'Z', {15, 1, 6, 8, 15}},
  };
  for (const auto &glyph : before) {
    if (glyph.character == 'Q' || glyph.character == 'X')
      continue;
    const auto &current = typography::c2Glyph(glyph.character);
    TEST_ASSERT_EQUAL(glyph.character, current.character);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(glyph.rows, current.rows, 5);
    TEST_ASSERT_EQUAL(
        glyph.character == 'I' ? 3 : (glyph.character == 'M' || glyph.character == 'W' ? 5 : 4),
        typography::c2Columns(glyph.character));
  }
}
void x_bitmap_and_spacing_are_precise() {
  const uint8_t expected[] = {9, 9, 6, 9, 9};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, typography::c2Glyph('X').rows, 5);
  TEST_ASSERT_EQUAL(4, typography::c2Columns('X'));
  TEST_ASSERT_EQUAL(4, typography::c2Width("X"));
  TEST_ASSERT_EQUAL(9, typography::c2Width("XX"));
}
void q_bowl_and_tail_change_are_precise() {
  const uint8_t before[] = {15, 9, 9, 11, 1};
  const uint8_t after[] = {15, 9, 9, 15, 2};
  const auto &q = typography::c2Glyph('Q');
  TEST_ASSERT_EQUAL_UINT8_ARRAY(after, q.rows, 5);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(before, q.rows, 3);
  TEST_ASSERT_EQUAL(4, before[3] ^ q.rows[3]); // Add row 4, column 2.
  TEST_ASSERT_EQUAL(3, before[4] ^ q.rows[4]); // Tail: column 4 off, column 3 on.
  TEST_ASSERT_EQUAL(4, typography::c2Columns('Q'));
  TEST_ASSERT_EQUAL(4, typography::c2Width("Q"));
}
void numerals_punctuation_and_normalization() {
  TEST_ASSERT_EQUAL(10, sizeof(typography::NumeralsC2) / sizeof(typography::NumeralsC2[0]));
  for (char character = '0'; character <= '9'; ++character) {
    const auto &glyph = typography::c2Glyph(character);
    TEST_ASSERT_EQUAL(character, glyph.character);
    const int columns = typography::c2Columns(character);
    TEST_ASSERT_EQUAL(character == '1' ? 3 : 4, columns);
    for (int row = 0; row < 5; ++row) {
      TEST_ASSERT_GREATER_THAN(0, glyph.rows[row]);
      TEST_ASSERT_LESS_THAN(1 << columns, glyph.rows[row]);
      if (row < 4)
        for (int col = 0; col < columns - 1; ++col) {
          const int pair = 3 << col;
          TEST_ASSERT_FALSE((glyph.rows[row] & pair) == pair &&
                            (glyph.rows[row + 1] & pair) == pair);
        }
    }
  }
  TEST_ASSERT_FALSE(std::equal(typography::c2Glyph('0').rows, typography::c2Glyph('0').rows + 5,
                               typography::c2Glyph('O').rows));
  TEST_ASSERT_FALSE(std::equal(typography::c2Glyph('5').rows, typography::c2Glyph('5').rows + 5,
                               typography::c2Glyph('S').rows));
  TEST_ASSERT_TRUE(typography::c2Supports("0123456789 '-./’"));
  TEST_ASSERT_EQUAL(2, typography::c2Columns('\''));
  TEST_ASSERT_EQUAL(3, typography::c2Columns('-'));
  TEST_ASSERT_EQUAL(1, typography::c2Columns('.'));
  TEST_ASSERT_EQUAL(3, typography::c2Columns('/'));
  TEST_ASSERT_EQUAL_STRING("PLACA D'ESPANYA", typography::c2Text("Plaça d’Espanya").c_str());
  TEST_ASSERT_EQUAL_STRING("GRACIA", typography::c2Text("Gràcia").c_str());
  TEST_ASSERT_EQUAL_STRING("D?HEBRON", pixelText("d’Hebron").c_str()); // Production unchanged.
  TEST_ASSERT_EQUAL_STRING("?", typography::c2Missing("‘–—🚇").c_str());
  TEST_ASSERT_EQUAL_STRING(",:!+?", typography::c2Missing(",:!+?").c_str());
  Framebuffer actual(128, 32), ascii(128, 32);
  typography::draw(actual, Face::C2, "Plaça d’Espanya", 0, 0, 1, 0, 128);
  typography::draw(ascii, Face::C2, "PLACA D'ESPANYA", 0, 0, 1, 0, 128);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 128; ++x)
      TEST_ASSERT_EQUAL(actual.pixel(x, y), ascii.pixel(x, y));
  for (const auto &text :
       {std::string("0123456789"), std::string("Aeroport T1"), std::string("Aeroport T2"),
        std::string("A '-./ B"), std::string("Plaça d’Espanya")}) {
    Framebuffer frame(128, 32);
    typography::draw(frame, Face::C2, text, 0, 0, 1, 0, 128);
    int right = 0;
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 128; ++x)
        if (frame.pixel(x, y))
          right = std::max(right, x + 1);
    TEST_ASSERT_EQUAL(typography::c2Width(text), right);
  }
  // A period remains on the same baseline; unsupported comma is visibly '?' elsewhere.
  Framebuffer period(64, 32);
  typography::draw(period, Face::C2, ".", 2, 21, 1, 2, 63);
  TEST_ASSERT_EQUAL(1, period.pixel(2, 25));
  TEST_ASSERT_EQUAL(0, period.pixel(2, 24));
}
void c2_dimensions_counters_and_coverage() {
  TEST_ASSERT_EQUAL(26, sizeof(typography::LabelsC2) / sizeof(typography::LabelsC2[0]));
  for (const auto &glyph : typography::LabelsC2) {
    const int columns = typography::c2Columns(glyph.character);
    TEST_ASSERT_EQUAL(
        glyph.character == 'I' ? 3 : (glyph.character == 'M' || glyph.character == 'W' ? 5 : 4),
        columns);
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
  TEST_ASSERT_TRUE(typography::c2Supports("Bus 23"));
  TEST_ASSERT_FALSE(typography::c2Supports("🚇"));
  Framebuffer actual(64, 32), fallback(64, 32);
  typography::draw(actual, Face::C2, ",", 0, 0, 1, 0, 64);
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
      // The offline diagnostic retains its historical countdown samples.
      // All pixels outside the arrival field still match the live renderer.
      const auto view = typography::sampleView(sample);
      const int left =
          1 + std::min(30, typography::c2Width(view.stops.front().groups.front().line) + 2) + 2;
      for (int y = 12; y < 17; ++y)
        for (int x = left; x < 63; ++x)
          expected.pixel(x, y, actual.pixel(x, y));
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
        if (face != Face::C2)
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
  state.command('f');
  TEST_ASSERT_EQUAL(typography::AlphabetSample, state.sample);
  state.command('g');
  TEST_ASSERT_EQUAL(typography::DestinationSample, state.sample);
  TEST_ASSERT_TRUE(state.command('?'));
  TEST_ASSERT_EQUAL(typography::DestinationSample, state.sample);
  TEST_ASSERT_FALSE(state.command(':'));
  state.command('p');
  TEST_ASSERT_EQUAL(typography::DestinationSample - 1, state.sample);
  TEST_ASSERT_TRUE(state.face == Face::C2);
  TEST_ASSERT_EQUAL(1600, state.phase());
  state.command('h');
  TEST_ASSERT_EQUAL(typography::SymbolSample, state.sample);
  TEST_ASSERT_TRUE(state.face == Face::C2);
  TEST_ASSERT_EQUAL(1600, state.phase());
  state.command('r');
  TEST_ASSERT_EQUAL(0, state.phase());
}
} // namespace
int main() {
  UNITY_BEGIN();
  RUN_TEST(sample_palette_matches_existing_fixture);
  RUN_TEST(original_c_labels_with_regular_badges);
  RUN_TEST(comparison_faces_with_regular_badges);
  RUN_TEST(established_c2_glyphs_are_unchanged);
  RUN_TEST(complete_alphabet_and_remaining_character_audit);
  RUN_TEST(validated_capitals_except_q_and_x_are_unchanged);
  RUN_TEST(x_bitmap_and_spacing_are_precise);
  RUN_TEST(q_bowl_and_tail_change_are_precise);
  RUN_TEST(numerals_punctuation_and_normalization);
  RUN_TEST(c2_dimensions_counters_and_coverage);
  RUN_TEST(c2_measured_width_clipping_and_reveal);
  RUN_TEST(focused_countdowns_are_identical);
  RUN_TEST(operational_baseline_is_exact);
  RUN_TEST(only_text_slots_change_and_badges_are_regular);
  RUN_TEST(exact_browser_shapes_and_spacing);
  RUN_TEST(normalization_clipping_and_reveal);
  RUN_TEST(serial_selection_is_repeatable);
  return UNITY_END();
}
