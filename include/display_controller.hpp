#pragma once

#include <cstdint>

#include "display_model.hpp"

#ifndef NEXSTAR_DISPLAY_ENABLED
#define NEXSTAR_DISPLAY_ENABLED 1
#endif

#if NEXSTAR_DISPLAY_ENABLED
#include <Adafruit_SSD1306.h>
#endif

namespace nexstar {

class DisplayController {
 public:
  DisplayController();

  bool begin(const DisplaySnapshot& snapshot, std::uint32_t now_ms);
  void update(const DisplaySnapshot& snapshot, std::uint32_t now_ms);
  [[nodiscard]] bool available() const { return available_; }

 private:
#if NEXSTAR_DISPLAY_ENABLED
  void renderBoot(const DisplaySnapshot& snapshot);
  void renderMain(const DisplaySnapshot& snapshot);
  void renderFault(const DisplaySnapshot& snapshot);

  Adafruit_SSD1306 display_;
  DisplaySnapshot last_snapshot_{};
  std::uint32_t boot_started_ms_{0};
  std::uint32_t last_render_ms_{0};
  bool showing_boot_{false};
#endif
  bool available_{false};
};

}  // namespace nexstar
