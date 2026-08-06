#pragma once

#include <cstdint>

#include "display_model.hpp"

namespace nexstar {

class DisplayController {
 public:
  DisplayController();

  bool begin(const DisplaySnapshot& snapshot, std::uint32_t now_ms);
  void update(const DisplaySnapshot& snapshot, std::uint32_t now_ms);
  [[nodiscard]] bool available() const { return available_; }

 private:
  void render(const DisplaySnapshot& snapshot);
  void renderStatusLine(const DisplaySnapshot& snapshot);

  bool available_{false};
  std::uint32_t last_render_ms_{0};
  DisplaySnapshot last_snapshot_{};
};

}  // namespace nexstar
