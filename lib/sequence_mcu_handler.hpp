 /*
 *  sequence_mcu_handler.hpp
 *
 *  Created on: November 11th, 2025
 *      Author: nick.hanna
 * 
 *  Internal Documentation:
 *      https://app.gitbook.com/o/YTmUofDhDJMJMEH1WaNo/s/StKxLxmDaS0JejoOwoAm
 *      /sequence-mcu-side-smbus-development-document-for-nxp-k32l-on-k800-r 
 * 
 *  Adheres to:
 *      https://google.github.io/styleguide/cppguide.html
 * 
 *  I2C dev version of functions:
 *      get state:
 *      i2cget -y 1 0x40 0 b
 *      
 *      get transition cause:
 *      i2cget -y 1 0x40 0x01 b
 *      
 *      get power state and transition cause:
 *      i2cget -y 1 0x40 0x02 w
 *      
 *      get capabilities:
 *      i2cget -y 1 0x40 0x03 b
 *      
 *      set state:
 *      i2cset -y 1 0x40 0x04 0x00 b
 *      i2cset -y 1 0x40 0x04 0x01 b
 *      i2cset -y 1 0x40 0x04 0x02 b
 *      i2cset -y 1 0x40 0x04 0x03 b
 *      i2cset -y 1 0x40 0x04 0x00 b
 *   
 *  Note: 
 *      Mappings from SMBUS datatypes to DBUS interfaces are 
 *      contained within chassis.cpp and host.cpp
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utility> // std::pair, std::to_underlying
#include <vector>

#include <boost/asio/spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <phosphor-logging/lg2.hpp>
#include <unordered_map>
#include <thread>

#include <chrono>
using namespace std::chrono_literals;

#include "smbus_manager.hpp"
PHOSPHOR_LOG2_USING;

#define SMBUS_RX_BUFFER_SIZE   2
#define STATE_AND_TCAUSE_SIZE  2
#define MAX_EVENT_LISTENERS    2

enum class TransitionCause : uint8_t {
    kTransitionCause_Unknown = 0x00,             // Unknown
    kTransitionCause_ChassisControl,             // Chassis Control command (BMC triggered)
    kTransitionCause_ResetPushbutton,            // Reset via pushbutton
    kTransitionCause_PowerUpPushbutton,          // Power-up via power pushbutton
    kTransitionCause_WatchdogExpiration,         // Watchdog expiration
    kTransitionCause_Oem,                        // OEM (TBD)
    kTransitionCause_AcRestoreAlways,            // Automatic power-up on AC being applied due to ‘always restore’ power restore policy
    kTransitionCause_AcRestorePrevious,          // Automatic power-up on AC being applied due to ‘restore previous power state’ power restore policy
    kTransitionCause_ResetPef,                   // Reset via PEF (Not used)
    kTransitionCause_PowerCyclePef,              // Power-cycle via PEF (Not used)
    kTransitionCause_SoftReset,                  // Soft reset (eg. CTRL-ALT-DEL)
    kTransitionCause_PowerUpRtc,                 // Power-up via RTC (system real time clock) wakeup
    kTransitionCause_IgnitionStartupTimer,       // Ignition Start Up Timer
    kTransitionCause_IgnitionSoftOffTimer,       // Ignition Soft Off Timer
    kTransitionCause_IgnitionHardOffTimer,       // Ignition Hard Off Timer
    kTransitionCause_IgnitionLowVoltageTimer,    // Ignition Low Voltage Timer
    kTransitionCause_BMCVirtualPowerButtonPress, // <In Use> Last Known Event was Virtual BMC Power Button Event
    kTransitionCause_BMCVirtualResetPress,       // <In Use> Last Known Event was Virtual BMC Reset Signal Event

    kTransitionCause_BMCSetValidationError,      // <Proposed> Error Notification
    kTransitionCause_BMCSetEventError,           // <Proposed> Error Notification
}; 

enum class CommandCode : uint8_t {
    GetPowerstate                           = 0x00, // Subaddress for [W/R]
    GetTransitionCause                      = 0x01, // Subaddress for [W/R]
    GetPowerStateAndTransitionCause         = 0x02, // Subaddress for [W/R] (2 Byte types: [power_state_t, transition_cause_t])
    GetCapabilities                         = 0x03, // Subaddress for [W/R]
    SetPowerState                           = 0x04, // Command code for [W], ex. [SetPowerState, kPowerEvent]
    UndefinedCode                           = 0xFF
};

enum class McuPowerState : uint8_t {
    kSLP_S0                                 = 0x00,
    kSLP_S3                                 = 0x03,
    kSLP_S4                                 = 0x04,
    kSLP_S5                                 = 0x05,
    kSLP_LP                                 = 0x06,
    kSLP_Unknown                            = 0xFF,
};

enum class PowerEvent : uint8_t {
    kPowerEvent_Awake                       = 0x00,
    kPowerEvent_HardReset                   = 0x01,
    kPowerEvent_SoftOff                     = 0x02,
    kPowerEvent_HardOff                     = 0x03,
    kPowerEvent_Unknown                     = 0xFF,
};

enum class SMBUSOperationStatus : uint8_t {
    kSMBUSOperationStatus_Success           = 0x00,
    kSMBUSOperationStatus_ProtocolError     = 0x01,
    kSMBUSOperationStatus_InvalidCommand    = 0x02,
    kSMBUSOperationStatus_InvalidRetries    = 0x02,
    kSMBUSOperationStatus_Undefined         = 0xFF,
};

enum class SMBUSCapability : uint8_t {
  kSmbusCapabilities_Unisolated             = 0x00,
  kSmbusCapabilities_Isolated               = 0x01,
  kSmbusCapabilities_Unknown                = 0xFF,
};

class SequenceMCUHandler {
    public:
        SequenceMCUHandler(SMBUSManager& smbus_init,
                           boost::asio::io_context& io);

        ~SequenceMCUHandler();

        std::vector<void(*)()> listener_handlers;
    /**
     * @brief Register a notification listener for MCU events.
     * @param listener_handler Function pointer to the event handler.
     * @return void
     */
    void RegisterNotification(void (*listener_handler)());

    /**
     * @brief Start polling the MCU for state changes and events.
     * @return void
     */
    void StartPolling();
        
    /**
     * @brief Issue an Awake command to the MCU via SMBus.
     * @param retries Number of retry attempts for the command.
     * @return SMBUSOperationStatus indicating success or error type.
     */
    SMBUSOperationStatus IssueAwakeCmd(uint8_t retries = 3);
    /**
     * @brief Issue a Soft Reset sequence to the MCU via SMBus.
     * @param retries Number of retry attempts for the command.
     * @return SMBUSOperationStatus indicating success or error type.
     */
    SMBUSOperationStatus IssueSoftReset(uint8_t retries = 3);
    /**
     * @brief Issue a Hard Reset command to the MCU via SMBus.
     * @param retries Number of retry attempts for the command.
     * @return SMBUSOperationStatus indicating success or error type.
     */
    SMBUSOperationStatus IssueHardReset(uint8_t retries = 3);
    /**
     * @brief Issue a Soft Shutdown command to the MCU via SMBus.
     * @param retries Number of retry attempts for the command.
     * @return SMBUSOperationStatus indicating success or error type.
     */
    SMBUSOperationStatus IssueSoftShutdown(uint8_t retries = 3);
    /**
     * @brief Issue a Hard Shutdown command to the MCU via SMBus.
     * @param retries Number of retry attempts for the command.
     * @return SMBUSOperationStatus indicating success or error type.
     */
    SMBUSOperationStatus IssueHardShutdown(uint8_t retries = 3);

        // Read Operations
    /**
     * @brief Read the current MCU power state from SMBus.
     * @param current_power_state Reference to store the read power state.
     * @return SMBUSOperationStatus indicating success or error type.
     */
    SMBUSOperationStatus GetMcuPowerState(McuPowerState& current_power_state);
    /**
     * @brief Read the last transition cause from the MCU via SMBus.
     * @param transition_cause Reference to store the read transition cause.
     * @return SMBUSOperationStatus indicating success or error type.
     */
    SMBUSOperationStatus GetTransitionCause(TransitionCause& transition_cause);
    /**
     * @brief Read the SMBus capability from the MCU.
     * @param get_capability Reference to store the read capability.
     * @return SMBUSOperationStatus indicating success or error type.
     */
    SMBUSOperationStatus GetCapability(SMBUSCapability& get_capability);
        
        // Helper for block reads
    /**
     * @brief Read both the MCU power state and transition cause in a single SMBus transaction.
     * @param state_cause_pair Reference to store the read state and cause as a pair.
     * @return SMBUSOperationStatus indicating success or error type.
     */
    SMBUSOperationStatus GetStateAndTransitionCause(std::pair<McuPowerState, TransitionCause>& state_cause_pair);

    /**
     * @brief Get the cached SMBus capability value.
     * @return SMBUSCapability Cached capability value.
     */
    inline SMBUSCapability GetSMBUSCapabilityCache() { return seq_mcu_ctx_.capabilities; };
    /**
     * @brief Get the cached MCU power state value.
     * @return McuPowerState Cached power state value.
     */
    inline McuPowerState GetMcuPowerStateCache() { return seq_mcu_ctx_.power_state; };
    /**
     * @brief Get the cached transition cause value.
     * @return TransitionCause Cached transition cause value.
     */
    inline TransitionCause GetTransitionCauseCache() { return seq_mcu_ctx_.transition_cause; };

        // Cache of important operational details
        struct SequenceMcuContext {
            SMBUSCapability capabilities;
            McuPowerState power_state;
            TransitionCause transition_cause;
        };

    private:
        SMBUSManager& sequence_smbus_instance_;
        SequenceMcuContext seq_mcu_ctx_;

        boost::asio::steady_timer poll_timer_;
        bool stop_dbus_refresh_{false};

    /**
     * @brief Internal: Run the polling loop for MCU state and transition cause.
     * @param last_state Last known MCU power state.
     * @param last_cause Last known transition cause.
     * @return void
     */
    void RunPollLoop(McuPowerState last_state, TransitionCause last_cause);

        // TODO: Atomic variable that will stop fired commands when a new one comes in
        //       if execution flow overlaps
        // std::atomic<bool> stop_write_operation_{true};

        inline SequenceMcuContext InitSequenceMcuContext(void) {
            SequenceMcuContext init_return = {
                .capabilities     = SMBUSCapability::kSmbusCapabilities_Unknown,
                .power_state      = McuPowerState::kSLP_Unknown,
                .transition_cause = TransitionCause::kTransitionCause_Unknown,                
            };

            return init_return;
    };
};

