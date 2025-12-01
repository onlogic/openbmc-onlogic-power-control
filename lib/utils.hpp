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

static inline int run_i2c_tests() {
    SMBUSManager smbus("/dev/i2c-1", 0x40);
    smbus.InitSMBUSManager();

    uint8_t output;
    int operation_status;

    operation_status = smbus.SmbusSubaddressReadByte(0x00, &output);
    if (operation_status < 0) {
        return -1;
    }
    info("GET state: {STATE}", "STATE", output);
    sleep(1);

    operation_status = smbus.SmbusSubaddressReadByte(0x01, &output);
    if (operation_status < 0) {
        return -1;
    }
    info("GET_TRANSITION_CAUSE: {GET_TRANSITION_CAUSE}", "GET_TRANSITION_CAUSE", output);
    sleep(1);

    operation_status = smbus.SmbusSubaddressReadByte(0x03, &output);
    if (operation_status < 0) {
        return -1;
    }
    info("GET_CAPABILITIES: {GET_CAPABILITIES}", "GET_CAPABILITIES", output);
    sleep(1);

    uint8_t buf[2];
    size_t size_read;
    operation_status = smbus.SmbusSubaddressReadByteBlock(0x02, 2, buf, &size_read);
    if (operation_status == 0) {
    info("BLOCK READ (subaddress 0x02): size_read={SIZE}, buf[0]={B0_DEC}, buf[1]={B1_DEC}",
         "SIZE", size_read,
         "B0_DEC", buf[0],
         "B1_DEC", buf[1]);
    } else {
        error("BLOCK READ failed: {STATUS}", "STATUS", operation_status);
    }

    // ***********************************************************************

    const uint8_t OPERATION_TESTS = 3; 
    uint8_t operation_tests[OPERATION_TESTS] = { 0x00, 0x01, 0x03 };
    uint8_t write_operation;
    uint8_t subaddress = 0x04;

    int write_status = 0;
    for(int i = 0; i < OPERATION_TESTS; ++i) {
        write_operation = operation_tests[i];
        info("OPERATION: {WRITE_OPERATION}", "WRITE_OPERATION", write_operation);
        write_status = smbus.SmbusWriteByte(subaddress, write_operation);
        info("Write Status: {WRITE_STATUS}", "WRITE_STATUS", write_status);
        sleep(5);
    }

    return 0;
}
