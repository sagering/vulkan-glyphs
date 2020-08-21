// clang-format off
#include <vulkan\vulkan_core.h>
#include <GLFW\glfw3.h>
// clang-format on

#include <glm\gtx\transform.hpp>

#include "window.h"
#include "renderer.h"
#include "clock.h"
#include "glyphs.h"

// aspect correction, scaling and y flipping of bezier control
// points
Renderer::ContourRenderObj
contourToRenderObj(std::vector<float>& c,
                   float xMin,
                   float yMin,
                   float xMax,
                   float yMax,
                   float z,
                   float aspectRatio,
                   float scale)
{
  for (int i = 0; i < c.size(); i += 2) {
    c[i] = (2 * (c[i] - xMin) / (xMax - xMin) - 1) * scale;
    c[i + 1] = (2 * (c[i + 1] - yMin) / (yMax - yMin) - 1) * scale;
    c[i + 1] = -c[i + 1];
  }

  std::vector<Renderer::Segment> segments;

  for (int i = 0; i < c.size() - 5; i += 4) {
    segments.push_back({ { c[i], c[i + 1], z },
                         { 0.f, 0.f },
                         { c[i + 2], c[i + 3], z },
                         { .5f, 0.f },
                         { c[i + 4], c[i + 5], z },
                         { 1.f, 1.f } });
  }

  std::vector<glm::vec3> fan;

  fan.push_back({ -1.f, -1.f, 0.f });

  for (auto seg : segments) {
    fan.push_back(seg.p0);
  }

  fan.push_back(segments[0].p0);

  return { segments, fan };
}

int
main()
{
  {
    Window window(1280, 920, "Quadratic Bezier Contours");
    Renderer renderer(&window);
    Clock clock = {};

    std::vector<Renderer::ContourRenderObj> contourRenderObjs;

    const float showTime = 1.f;
    float timePassed = 0.f;
    int glyphIdx = 0;

    while (window.keyboardState.key[GLFW_KEY_ESCAPE] != 1) {
      window.Update();
      renderer.Update();
      clock.Update();

      timePassed += clock.GetTick();

      // select next glyph
      if (timePassed > showTime) {
        glyphIdx = (glyphIdx + 1) % glyphs.size();
        timePassed = 0.f;
      }

      contourRenderObjs.clear();

      // tranform glyph data to render primitives (segments + triangle fans)
      for (auto c : glyphs[glyphIdx].contours) {
        contourRenderObjs.push_back(contourToRenderObj(
          c,
          glyphs[glyphIdx].xMin,
          glyphs[glyphIdx].yMin,
          glyphs[glyphIdx].xMax,
          glyphs[glyphIdx].yMax,
          0.f,
          window.windowSize.width / (float)window.windowSize.height,
          0.1f + 0.3f * timePassed / showTime));
      }

      // hand renderable primitives to renderer (first all segments, then all
      // fans)
      for (auto& cro : contourRenderObjs)
        renderer.pushSegments(cro.segments);

      for (auto& cro : contourRenderObjs)
        renderer.pushFan(cro.fan);

      renderer.drawFrame();
    }
  }

  return 0;
}
