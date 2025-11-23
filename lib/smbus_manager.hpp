/*
 * smbus_manager.hpp
 *
 *  Created on: November 11th, 2025
 *      Author: nick.hanna
 * 
 *  Internal Documentation:
 *      https://app.gitbook.com/o/YTmUofDhDJMJMEH1WaNo/s/StKxLxmDaS0JejoOwoAm
 *      /sequence-mcu-side-smbus-development-document-for-nxp-k32l-on-k800-r 
 * 
 *  Adheres to:
 *      Traceable to POSIX user-space syntax and style for C interfaces, 
 *      https://app.gitbook.com/o/YTmUofDhDJMJMEH1WaNo/s/StKxLxmDaS0JejoOwoAm for C++
 *  
 *  Description:
 *      Low-Level Endpoint class that handles C-Like Transactions, designed to be injectable
 *      into SequenceMCUHandler. 
 * 
 *  Also See:
 *      https://www.kernel.org/doc/Documentation/i2c/dev-interface
 *      https://android.googlesource.com/kernel/common/+/refs/tags/android12-5.10-2022-02_r7/drivers/i2c/i2c-core-smbus.c#:~:text=from%20the%20device.-,*%2F,%7D
 *      libi2c
 *      https://linux.kernel.narkive.com/PeNlDZ1K/i2c-smbus-question
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <errno.h>
#include <unistd.h>

#include <string>
#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

enum class SlaveAddressTable : uint8_t {
    kSlaveAddress_PoeModbay1      = 0x20,
    kSlaveAddress_PoeModbay0      = 0x28,
    //kSlaveAddress_MiscAddress   = 0x36,
    kSlaveAddress_SequenceMCU     = 0x40,
    kSlaveAddress_PoeDaughterCard = 0x41,
    kSlaveAddress_BmcChip         = 0x44,
    kSlaveAddress_Ram0            = 0x50,
    kSlaveAddress_Ram1            = 0x51,
    kSlaveAddress_DDR5Write       = 0xA0,
    kSlaveAddress_DDR5Read        = 0xA1
};

class SMBUSManager {
    public: 
        SMBUSManager(const std::string& i2c_device, uint8_t device_address);
        ~SMBUSManager();
        
        /**
         * @brief Initialize the SMBUS manager and open the I2C device.
         * @return int 0 on success, negative value on error.
         */
        int InitSMBUSManager();

        /**
         * @brief Read a single byte from a given SMBus subaddress.
         * @param subaddress The SMBus subaddress to read from.
         * @param resultant Pointer to store the read byte.
         * @return int 0 on success, negative value on error.
         */
        int SmbusSubaddressReadByte(uint8_t subaddress, uint8_t* resultant);
        /**
         * @brief Read a block of bytes from a given SMBus subaddress.
         * @param subaddress The SMBus subaddress to read from.
         * @param size_requested Number of bytes to read.
         * @param resultant Pointer to buffer to store the read bytes.
         * @param size_read Pointer to store the actual number of bytes read.
         * @return int 0 on success, negative value on error.
         */
        int SmbusSubaddressReadByteBlock(uint8_t subaddress, size_t size_requested, uint8_t* resultant, size_t* size_read);

        /**
         * @brief Write a single byte to a given SMBus subaddress.
         * @param subaddress The SMBus subaddress to write to.
         * @param value The byte value to write.
         * @return int 0 on success, negative value on error.
         */
        int SmbusWriteByte(uint8_t subaddress, uint8_t value);
        /**
         * @brief Write multiple bytes to a given SMBus subaddress.
         * @param subaddress The SMBus subaddress to write to.
         * @param value_arr Pointer to array of bytes to write.
         * @param size_value_arr Number of bytes to write from the array.
         * @return int 0 on success, negative value on error.
         */
        int SmbusWriteByteMulti(uint8_t subaddress, uint8_t* value_arr, size_t size_value_arr);

    private:
        std::string i2c_device_ = "";
        uint8_t device_address_ = 0;
        int fd_ = -1;
};
