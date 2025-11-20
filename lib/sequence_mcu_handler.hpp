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
 */

#ifndef SEQUENCE_MCU_HANDLER_HPP_
#define SEQUENCE_MCU_HANDLER_HPP_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utility> // std::pair, std::to_underlying

#include <xyz/openbmc_project/State/Host/common.hpp>
#include <xyz/openbmc_project/State/Chassis/common.hpp>

#include <phosphor-logging/lg2.hpp>
#include <unordered_map>
#include <mutex>
#include <thread>

#include <chrono>
using namespace std::chrono_literals;

#include "smbus_manager.hpp"
PHOSPHOR_LOG2_USING;

#define SMBUS_RX_BUFFER_SIZE   2
#define STATE_AND_TCAUSE_SIZE  2

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

enum class PowerState : uint8_t {
    kSLP_S0                                 = 0x00,
    kSLP_S3                                 = 0x03,
    kSLP_S4                                 = 0x04,
    kSLP_S5                                 = 0x05,
    kSLP_LP                                 = 0x06,
    kSLP_Unknown                            = 0xFF,
};

enum class PowerEvent : uint8_t {
    kPowerEvent_Awake                      = 0x00,
    kPowerEvent_HardReset                  = 0x01,
    kPowerEvent_SoftOff                    = 0x02,
    kPowerEvent_HardOff                    = 0x03,
    kPowerEvent_Unknown                    = 0xFF,
};

enum class SMBUSOperationStatus : uint8_t {
    kSMBUSOperationStatus_Success          = 0x00,
    kSMBUSOperationStatus_ProtocolError    = 0x01,
    kSMBUSOperationStatus_InvalidCommand   = 0x02,
    kSMBUSOperationStatus_InvalidRetries   = 0x02,
    kSMBUSOperationStatus_Undefined        = 0xFF,
};

enum class SMBUSCapability : uint8_t {
  kSmbusCapabilities_Unisolated            = 0,
  kSmbusCapabilities_Isolated              = 1,
  kSmbusCapabilities_Unknown               = 0xFF,
};

class SequenceMCUHandler {
    public:
        using Host = sdbusplus::common::xyz::openbmc_project::state::Host;
        using Chassis = sdbusplus::common::xyz::openbmc_project::state::Chassis;

        // https://google.github.io/styleguide/cppguide.html#Implicit_Conversions
        explicit SequenceMCUHandler(SMBUSManager& smbus_init) :
            sequence_smbus_instance_(smbus_init),
            seq_mcu_ctx_(InitSequenceMcuContext()) {}

        ~SequenceMCUHandler() {}

        SMBUSOperationStatus IssueAwakeCmd(uint8_t retries = 3);
        SMBUSOperationStatus IssueSoftReset(uint8_t retries = 3);
        SMBUSOperationStatus IssueHardReset(uint8_t retries = 3);
        SMBUSOperationStatus IssueSoftShutdown(uint8_t retries = 3);
        SMBUSOperationStatus IssueHardShutdown(uint8_t retries = 3);

        SMBUSOperationStatus GetPowerState(Host::HostState& current_power_state);
        SMBUSOperationStatus GetTransitionCause(Host::RestartCause& transition_cause);
        SMBUSOperationStatus GetStateAndTransitionCause(std::pair<Host::HostState, Host::RestartCause>& gst_pair);
        
         // Chassis Operations
        SMBUSOperationStatus GetChassisPowerState(Chassis::PowerState& current_power_state);
        SMBUSOperationStatus GetChassisPowerStatus(Chassis::PowerStatus& power_status);
        SMBUSOperationStatus GetChassisStateAndPowerStatus(std::pair<Chassis::PowerState, Chassis::PowerStatus>& state_status_pair);

        SMBUSOperationStatus GetCapability(SMBUSCapability& get_capability);

        // make public just in case, static for one instance overall regardless of num
        static const std::unordered_map<Host::HostState, uint8_t> hostStateToNative;
        static const std::unordered_map<uint8_t, Host::HostState> nativeToHostState;
        static const std::unordered_map<uint8_t, SMBUSCapability> validCapabilities;
        static const std::unordered_map<uint8_t, Host::RestartCause> mapTransitionCauseIPMItoOBMC;

        static const std::unordered_map<uint8_t, Chassis::PowerState> nativeToChassisState;
        static const std::unordered_map<Chassis::PowerState, uint8_t> chassisStateToNative;
        static const std::unordered_map<Chassis::Transition, uint8_t> chassisTransitionToNative;
        static const std::unordered_map<uint8_t, Chassis::PowerStatus> mapTransitionCauseToChassisPowerStatus;

        // Cache of important operational details
        struct SequenceMcuContext {
            SMBUSCapability capabilities;

            Host::Transition power_state_to_transmit;
            Host::RestartCause last_known_transition_cause;
            Host::HostState last_known_power_state;

            Chassis::Transition chassis_transition_to_transmit;
            Chassis::PowerState last_known_chassis_power_state;
            Chassis::PowerStatus last_known_chassis_power_status;
        };

        inline SMBUSCapability GetSMBUSCapabilityCache() { return seq_mcu_ctx_.capabilities; };   

        inline Host::Transition GetPowerStateToTransmitCache() { return seq_mcu_ctx_.power_state_to_transmit; };
        inline Host::RestartCause GetRestartCauseCache() { return seq_mcu_ctx_.last_known_transition_cause; };
        inline Host::HostState GetLastKnownPowerStateCache() { return seq_mcu_ctx_.last_known_power_state; };

        inline Chassis::Transition GetChassisTransitionCache() { return seq_mcu_ctx_.chassis_transition_to_transmit; };
        inline Chassis::PowerState GetLastKnownChassisPowerStateCache() { return seq_mcu_ctx_.last_known_chassis_power_state; };
        inline Chassis::PowerStatus GetLastKnownChassisPowerStatusCache() { return seq_mcu_ctx_.last_known_chassis_power_status; };

        // TODO: make sure two threads don't access ioctl at same time during smbus writes
        std::mutex ioctl_lock{}; // ioctl_lock.load(); ioctl_lock.exchange(false); 
        
        // TODO: Atomic variable that will stop fired commands when a new one comes in
        //       if execution flow overlaps
        std::atomic<bool> should_stop{false};

    private:
        SMBUSManager& sequence_smbus_instance_;
        SequenceMcuContext seq_mcu_ctx_;

        inline SequenceMcuContext InitSequenceMcuContext(void) {
            SequenceMcuContext init_return = {
                .capabilities                = SMBUSCapability::kSmbusCapabilities_Unknown,

                // Host Defaults
                .power_state_to_transmit     = Host::Transition::Off,
                .last_known_transition_cause = Host::RestartCause::Unknown,
                .last_known_power_state      = Host::HostState::Quiesced,

                // Chassis Defaults
                .chassis_transition_to_transmit  = Chassis::Transition::Off,
                .last_known_chassis_power_state  = Chassis::PowerState::Off,
                .last_known_chassis_power_status = Chassis::PowerStatus::Undefined
            };

            return init_return;
        };
    };

#endif // SEQUENCE_MCU_HANDLER_HPP_
