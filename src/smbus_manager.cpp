#include "smbus_manager.hpp"

SMBUSManager::SMBUSManager(const std::string& i2c_device, uint8_t device_address) 
    : i2c_device_(i2c_device), device_address_(device_address), fd_(-1) {}

SMBUSManager::~SMBUSManager() {
    if (fd_ >= 0) {
        info("SMBUSManager :: Destroying FD");
        close(fd_);
    }
}

int SMBUSManager::InitSMBUSManager() {
    if (fd_ >= 0) {
        info("SMBUSManager :: The FD is already in use.");
        return 0;
    }

    fd_ = open(i2c_device_.c_str(), O_RDWR);
    if (fd_ < 0) {
        error("SMBUSManager :: ioctl failed: {ERROR_NO}", "ERROR_NO", strerror(errno));
        return -1;
    }

    info("SMBUSManager :: Successfully initialized FD");

    return 0;
}

int SMBUSManager::SmbusSubaddressReadByte(uint8_t subaddress, uint8_t* resultant) {
    if (fd_ < 0) {
        errno = ENXIO;
        error("SMBUSManager :: FD error, did you correctly initialize the SMBUS?: {ERROR_NO}",
              "ERROR_NO", errno);
        return -1;
    }

    if (ioctl(fd_, I2C_SLAVE, device_address_) < 0) {
        error("SMBUSManager :: ioctl failed: {ERROR_NO}", "ERROR_NO", errno);
        return -1;
    }

    debug("SMBUSManager :: Read Address {ADDRESS},  subaddress {SUBADDRESS}", 
          "ADDRESS"   , device_address_,
          "SUBADDRESS", subaddress);

    union i2c_smbus_data data;
    struct i2c_smbus_ioctl_data subaddress_read_byte_settings;

    subaddress_read_byte_settings.read_write = I2C_SMBUS_READ;
    subaddress_read_byte_settings.command = subaddress;
    subaddress_read_byte_settings.size = I2C_SMBUS_BYTE_DATA;
    subaddress_read_byte_settings.data = &data;

    int ret = ioctl(fd_, I2C_SMBUS, &subaddress_read_byte_settings);
    if (ret != 0) {
        error("SMBUSManager :: single byte read operation failed: {ERROR_NO}", "ERROR_NO", errno);
        return -1;
    }
    *resultant = data.byte;

    info("SMBUSManager :: Read subaddress {SUBADDR} (decimal), value = {VAL} (decimal)",
         "SUBADDR", subaddress, "VAL", *resultant);

    return 0;
}

int SMBUSManager::SmbusSubaddressReadByteBlock(uint8_t subaddress, size_t size_requested, uint8_t* resultant, size_t* size_read) {
    if (fd_ < 0) {
        errno = ENXIO;
        error("SMBUSManager :: FD error, did you correctly initialize the SMBUS?: {ERROR_NO}",
              "ERROR_NO", errno);
        return -1;
    }

    if (ioctl(fd_, I2C_SLAVE, device_address_) < 0) {
        error("SMBUSManager :: ioctl failed: {ERROR_NO}", "ERROR_NO", errno);
        return -1;
    }

    if (size_requested > I2C_SMBUS_BLOCK_MAX) {
        errno = E2BIG;
        error("SMBUSManager :: size_requested too large for max SMBUS multi-byte read operation: {ERROR_NO}", 
              "ERROR_NO", strerror(errno));
        return -1;
    }

    debug("SMBUSManager :: Read Address {ADDRESS},  subaddress {SUBADDRESS}", 
          "ADDRESS"   , device_address_,
          "SUBADDRESS", subaddress);

    union i2c_smbus_data data;
    struct i2c_smbus_ioctl_data subaddress_read_byte_settings;

    subaddress_read_byte_settings.read_write = I2C_SMBUS_READ;
    subaddress_read_byte_settings.command = subaddress;
    subaddress_read_byte_settings.size = I2C_SMBUS_I2C_BLOCK_DATA;
    subaddress_read_byte_settings.data = &data;
    data.block[0] = size_requested;

    int ret = ioctl(fd_, I2C_SMBUS, &subaddress_read_byte_settings);
    if (ret != 0) {
        error("SMBUSManager :: block read operation failed: {ERROR_NO}", "ERROR_NO", errno);
        return -1;
    }
    *size_read = (size_t)data.block[0];
    memcpy(resultant, &data.block[1], *size_read);

    for (size_t i = 0; i < *size_read; ++i) {
        debug("RESULTANT INDEX: {INDEX} VAL: {VAL}", "INDEX", i, "VAL", resultant[i]);
    }

    return 0;
}

int SMBUSManager::SmbusWriteByte(uint8_t subaddress, uint8_t value) {
    if (fd_ < 0) {
        errno = ENXIO;
        error("SMBUSManager :: FD error, did you correctly initialize the SMBUS: {ERROR_NO}",
              "ERROR_NO", strerror(errno));
        return -1;
    }

    if (ioctl(fd_, I2C_SLAVE, device_address_) < 0) {
        error("SMBUSManager :: ioctl failed: {ERROR_NO}", "ERROR_NO", strerror(errno));
        return -1;
    }

    union i2c_smbus_data data;
    data.byte = value;

    struct i2c_smbus_ioctl_data byte_write_settings;
    byte_write_settings.read_write = I2C_SMBUS_WRITE;
    byte_write_settings.command = subaddress;
    byte_write_settings.size = I2C_SMBUS_BYTE_DATA;
    byte_write_settings.data = &data;

    int ret = ioctl(fd_, I2C_SMBUS, &byte_write_settings);
    if (ret < 0) {
        error("SMBUSManager :: ioctl write-byte failed: {ERROR_NO}", "ERROR_NO", errno);
        return -1;
    }

    return 0;
}

int SMBUSManager::SmbusWriteByteMulti(uint8_t subaddress, uint8_t* value_arr, size_t size_value_arr) {
    if (fd_ < 0) {
        errno = ENXIO; 
        error("SMBUSManager :: FD error, did you correctly initialize the SMBUS: {ERROR_NO}", 
              "ERROR_NO", strerror(errno));
        return -1;
    }

    if (ioctl(fd_, I2C_SLAVE, device_address_) < 0) {
        error("SMBUSManager :: ioctl failed: {ERROR_NO}", "ERROR_NO", strerror(errno));
        return -1;
    }

    if (size_value_arr > I2C_SMBUS_BLOCK_MAX) {
        errno = E2BIG;
        error("SMBUSManager :: size_value_arr size too large for max SMBUS multi-byte write operation: {ERROR_NO}", 
              "ERROR_NO", strerror(errno));
        return -1;
    }

    union i2c_smbus_data data;
    data.block[0] = size_value_arr;
    for (size_t size_iter = 0; size_iter < size_value_arr; ++size_iter) {
        data.block[size_iter + 1] = value_arr[size_iter];
    }

    struct i2c_smbus_ioctl_data subaddress_read_byte_settings;
    subaddress_read_byte_settings.read_write = I2C_SMBUS_WRITE;
    subaddress_read_byte_settings.command = subaddress;
    subaddress_read_byte_settings.size = I2C_SMBUS_BLOCK_DATA;
    subaddress_read_byte_settings.data = &data;

    int ret = ioctl(fd_, I2C_SMBUS, &subaddress_read_byte_settings);
    if (ret < 0) {
        error("SMBUSManager :: ioctl multi-write byte failed: {ERROR_NO}", "ERROR_NO", errno);
        return -1;
    }

    return 0;
}
