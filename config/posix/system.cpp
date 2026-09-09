#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

#include "app.hpp"
#include "hardware.hpp"

int main() {
  if (const auto result = hardware::initialize(); !result) {
    std::fprintf(stderr, "hardware initialization failed: %u\n",
                 static_cast<unsigned>(result.error()));
    return 1;
  }
  if (!app::initialize()) {
    std::fputs("application initialization failed\n", stderr);
    return 2;
  }

  vTaskStartScheduler();
#if defined(ECU_HOST_TEST_DURATION_TICKS)
  return 0;
#else
  std::fputs("FreeRTOS scheduler returned unexpectedly\n", stderr);
  return 3;
#endif
}
