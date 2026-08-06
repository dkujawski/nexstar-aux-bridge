#include "display_service.hpp"

#if NEXSTAR_DISPLAY_ENABLED

#include <Arduino.h>

namespace nexstar {

bool DisplayService::begin(const DisplaySnapshot& initial_snapshot) {
  if (task_ != nullptr) {
    return true;
  }
  initial_snapshot_ = initial_snapshot;
  queue_ = xQueueCreateStatic(1, sizeof(DisplaySnapshot), queue_storage_,
                              &queue_control_);
  if (queue_ == nullptr) {
    return false;
  }
  task_ = xTaskCreateStaticPinnedToCore(
      TaskEntry, "tft", kTaskStackDepth, this, kTaskPriority, task_stack_,
      &task_control_, 0);
  return task_ != nullptr;
}

void DisplayService::publish(const DisplaySnapshot& snapshot) {
  if (queue_ != nullptr) {
    xQueueOverwrite(queue_, &snapshot);
  }
}

void DisplayService::TaskEntry(void* context) {
  static_cast<DisplayService*>(context)->run();
}

void DisplayService::run() {
  DisplaySnapshot current = initial_snapshot_;
  controller_.begin(current, millis());

  for (;;) {
    DisplaySnapshot update{};
    if (xQueueReceive(queue_, &update, pdMS_TO_TICKS(50)) == pdTRUE) {
      current = update;
    }
    controller_.update(current, millis());
  }
}

}  // namespace nexstar

#else

namespace nexstar {

bool DisplayService::begin(const DisplaySnapshot&) {
  return false;
}

void DisplayService::publish(const DisplaySnapshot&) {}

}  // namespace nexstar

#endif
