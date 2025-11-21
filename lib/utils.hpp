#pragma once

#include <cstdint>
#include <ctime>
#include <unistd.h> // Required for sleep()

// Headers required for the tests
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>
#include "smbus_manager.hpp"
#include "sequence_mcu_handler.hpp"

static inline uint64_t getCurrentTimeMs() {
    struct timespec time = {};

    if (clock_gettime(CLOCK_REALTIME, &time) < 0) {
        return 0;
    }

    uint64_t currentTimeMs = static_cast<uint64_t>(time.tv_sec) * 1000;
    currentTimeMs += static_cast<uint64_t>(time.tv_nsec) / 1000 / 1000;

    return currentTimeMs;
}
