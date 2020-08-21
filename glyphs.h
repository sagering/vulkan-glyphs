#pragma once
#include <vector>

struct Glyph
{
  const char* name;
  float xMin, yMin, xMax, yMax;
  std::vector<std::vector<float>> contours;
};

extern std::vector<Glyph> glyphs;
