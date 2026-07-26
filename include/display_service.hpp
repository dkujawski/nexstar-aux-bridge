#pragma once

#include "display_controller.hpp"
#include "display_model.hpp"

#if NEXSTAR_DISPLAY_ENABLED
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif

namespace nexstar {

class DisplayService {
 public:
  bool begin(const DisplaySnapshot& initial_snapshot);
  void publish(const DisplaySnapshot& snapshot);

 private:
#if NEXSTAR_DISPLAY_ENABLED
  static void TaskEntry(void* context);
  void run();

  static constexpr std::uint32_t kTaskStackDepth = 2048;
  static constexpr UBaseType_t kTaskPriority = 1;

  DisplayController controller_{};
  DisplaySnapshot initial_snapshot_{};
  StaticQueue_t queue_control_{};
  std::uint8_t queue_storage_[sizeof(DisplaySnapshot)]{};
  QueueHandle_t queue_{nullptr};
  StaticTask_t task_control_{};
  StackType_t task_stack_[kTaskStackDepth]{};
  TaskHandle_t task_{nullptr};
#endif
};

}  // namespace nexstar
