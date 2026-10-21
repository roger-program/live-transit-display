#pragma once
#include "renderer.h"
#include "typography_c2.h"
#include "typography_glyphs.h"
#include <algorithm>
#include <cstring>

namespace typography {
enum class Face { A, B, C, D, C2 };
struct Sample {
  const char *mode;
  const char *stop;
  const char *line;
  const char *destination;
  const char *background;
  const char *foreground;
  bool labelsOnly = false;
};
// Offline inputs only: existing reviewed Metro/API and TMB bus design palette.
// These synthetic predictions and name pairings are not verified live services.
inline constexpr Sample Samples[] = {
    {"metro", "Lesseps", "L3", "Zona Universitària", "#1EB53A", "#FFFFFF"},
    {"metro", "Passeig de Gràcia", "L4", "Trinitat Nova", "#F7A30E", "#000000"},
    {"metro", "Collblanc", "L9S", "Aeroport T1", "#FB712B", "#000000"},
    {"bus", "Plaça d'Espanya", "H8", "Parc de la Vall d'Hebron", "#2D4191", "#FFFFFF"},
    {"bus", "Pg. Marítim - Hospital del Mar", "V23", "Can Marcet - Horta", "#547B31", "#FFFFFF"},
    {"bus", "Gran Via - Balmes", "6", "Pg. Manuel Girona", "#C62A0F", "#FFFFFF"},
    // Focused label trials: fixed badge/header, identical operational countdowns.
    // Destination strings are reading stimuli, not claimed L3 service patterns.
    {"metro", "Lesseps", "L3", "Lesseps", "#1EB53A", "#FFFFFF", true},
    {"metro", "Lesseps", "L3", "La Pau", "#1EB53A", "#FFFFFF", true},
    {"metro", "Lesseps", "L3", "Trinitat Nova", "#1EB53A", "#FFFFFF", true},
    {"metro", "Lesseps", "L3", "Zona Universitària", "#1EB53A", "#FFFFFF", true},
    // Alphabet groups fit the existing 61-pixel destination slot without reveal.
    {"metro", "Lesseps", "L3", "ABCDEFGH", "#1EB53A", "#FFFFFF", true},
    {"metro", "Lesseps", "L3", "IJKLMNOPQ", "#1EB53A", "#FFFFFF", true},
    {"metro", "Lesseps", "L3", "RSTUVWXYZ", "#1EB53A", "#FFFFFF", true},
    {"metro", "Lesseps", "L3", "Bon Pastor", "#1EB53A", "#FFFFFF", true},
    {"metro", "Lesseps", "L3", "Collblanc", "#1EB53A", "#FFFFFF", true},
    {"metro", "Lesseps", "L3", "Badalona Pompeu Fabra", "#1EB53A", "#FFFFFF", true},
    {"metro", "Lesseps", "L3", "Gorg", "#1EB53A", "#FFFFFF", true},
};
inline constexpr unsigned AlphabetSample = 10, DestinationSample = 13;
inline constexpr unsigned SampleCount = sizeof(Samples) / sizeof(Samples[0]);
inline constexpr int64_t Now = 1800000000000LL;
inline transit::View sampleView(unsigned index) {
  const auto &sample = Samples[index % SampleCount];
  transit::Stop stop;
  stop.mode = sample.mode;
  stop.id = "typography-fixture";
  stop.name = sample.stop;
  stop.nameKnown = true;
  stop.status = transit::StopStatus::Ok;
  transit::Group group;
  group.line = sample.line;
  group.lineColor = sample.background;
  group.lineTextColor = sample.foreground;
  group.destination = sample.destination;
  for (int minutes : {2, 9}) {
    transit::Arrival arrival;
    arrival.arrivalAt = Now + minutes * 60000;
    arrival.provenance = transit::Provenance::Realtime;
    group.arrivals.push_back(arrival);
  }
  stop.groups.push_back(group);
  return {{stop}, ""};
}
inline const char *faceName(Face face) {
  switch (face) {
  case Face::A:
    return "A operational 5x5";
  case Face::B:
    return "B browser contemporary";
  case Face::C:
    return "C browser geometric";
  case Face::D:
    return "D browser condensed";
  case Face::C2:
    return "C2 geometric A-Z";
  }
  return "A operational 5x5";
}
inline const uint8_t *rowsFor(Face face, char character, bool wide) {
  const auto *labels = face == Face::B ? LabelsB : face == Face::C ? LabelsC : LabelsD;
  const auto *digits = face == Face::B ? CountdownsB : face == Face::C ? CountdownsC : CountdownsD;
  if (wide && character >= '0' && character <= '9')
    return digits[character - '0'].rows;
  for (unsigned i = 0; i < 36; ++i)
    if (labels[i].character == character)
      return labels[i].rows;
  for (const auto &glyph : Punctuation)
    if (glyph.character == character)
      return glyph.rows;
  for (const auto &glyph : Punctuation)
    if (glyph.character == '?')
      return glyph.rows;
  return Punctuation[0].rows;
}
inline int glyphWidth(Face face, char character, bool wide) {
  if (wide && character >= '0' && character <= '9')
    return 5;
  if (character == ' ')
    return 0;
  if (character == 'I')
    return 1;
  return face == Face::D && std::strchr("CEFJLS", character) ? 2 : 3;
}
inline int width(Face face, const std::string &text, bool arrivals = false) {
  if (face == Face::A)
    return transit::textWidth(text);
  if (face == Face::C2)
    return arrivals ? transit::textWidth(text) : c2Width(text);
  int result = -1;
  bool first = arrivals;
  for (char character : transit::pixelText(text)) {
    result += glyphWidth(face, character, first) + 1;
    if (character == ' ')
      first = false;
  }
  return std::max(0, result);
}
inline void draw(transit::Framebuffer &frame, Face face, const std::string &text, int x, int y,
                 transit::Color color, int left, int right, int offset = 0, bool arrivals = false) {
  if (face == Face::C2 && !arrivals) {
    drawC2(frame, text, x, y, color, left, right, offset);
    return;
  }
  if (face == Face::A || face == Face::C2) {
    transit::drawText(frame, text, x, y, color, left, right, offset);
    return;
  }
  bool first = arrivals;
  for (char character : transit::pixelText(text)) {
    const int columns = glyphWidth(face, character, first);
    const auto *rows = rowsFor(face, character, first);
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < columns; ++col) {
        const int px = x + col - offset;
        if (px >= left && px < right && (rows[row] & (1 << (columns - 1 - col))))
          frame.pixel(px, y + row, color);
      }
    x += columns + 1;
    if (character == ' ')
      first = false;
  }
}
inline void render(transit::Framebuffer &frame, Face face, unsigned sample, uint64_t phase) {
  auto view = sampleView(sample);
  transit::ArrivalRenderer baseline;
  baseline.render(frame, view, Now, phase);
  if (face == Face::A)
    return; // Actual production renderer, no reconstructed baseline.
  const auto &stop = view.stops.front();
  const auto &group = stop.groups.front();
  // Only the three existing text slots are replaced. All badges, separators,
  // backgrounds and border pixels remain the exact operational raster.
  frame.rect(10, 2, 53, 5, 0);
  frame.rect(2, 21, 61, 5, 0);
  const int slotLeft = 1 + std::min(30, transit::textWidth(group.line) + 2) + 2;
  if (!Samples[sample % SampleCount].labelsOnly && face != Face::C2)
    frame.rect(slotLeft, 12, 63 - slotLeft, 5, 0);
  draw(frame, face, stop.name, 10, 2, 0xe8eee8, 10, 63,
       transit::overflowOffset(width(face, stop.name), 53, phase));
  draw(frame, face, group.destination, 2, 21, 0xe8eee8, 2, 63,
       transit::overflowOffset(width(face, group.destination), 61, phase));
  // Focused samples leave the operational countdown raster entirely untouched.
  if (Samples[sample % SampleCount].labelsOnly || face == Face::C2)
    return;
  // Same immutable arrival content and operational right edge; first number uses
  // the browser's 5-column master, second uses its compact label digit.
  const std::string arrivals =
      transit::countdown(group.arrivals[0], Now) + " " + transit::countdown(group.arrivals[1], Now);
  draw(frame, face, arrivals, 63 - width(face, arrivals, true), 12, 0xaaff66, slotLeft, 63, 0,
       true);
}
struct Selection {
  Face face = Face::A;
  unsigned sample = 6;
  unsigned step = 0;
  static constexpr uint64_t Phases[] = {0, 1000, 1600, 2300};
  uint64_t phase() const { return Phases[step]; }
  bool command(int key) {
    if (key >= 'a' && key <= 'd')
      face = static_cast<Face>(key - 'a');
    else if (key == 'e')
      face = Face::C2;
    else if (key >= '0' && key <= '9')
      sample = key - '0';
    else if (key == 'f')
      sample = AlphabetSample;
    else if (key == 'g')
      sample = DestinationSample;
    else if (key == 'n')
      sample = (sample + 1) % SampleCount;
    else if (key == 'p')
      sample = (sample + SampleCount - 1) % SampleCount;
    else if (key == 's')
      step = (step + 1) % 4;
    else if (key == 'r')
      step = 0;
    else if (key != 'i' && key != '?')
      return false;
    return true;
  }
};
} // namespace typography
