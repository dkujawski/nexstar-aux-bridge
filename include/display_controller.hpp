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
  void renderBoot(const DisplaySnapshot& snapshot);
  void renderMain(const DisplayViewModel& view);
  void renderFault(const DisplayViewModel& view);
  static bool ViewsEqual(const DisplayViewModel& lhs,
                         const DisplayViewModel& rhs);

  bool available_{false};
  DisplayModel model_{};
  DisplayViewModel last_view_{};
  bool showing_boot_{false};
  std::uint32_t boot_started_ms_{0};
  std::uint32_t last_render_ms_{0};
};

}  // namespace nexstar
