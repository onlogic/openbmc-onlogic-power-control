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
#include <phosphor-logging/lg2.hpp>
#include <unordered_map>

#include "smbus_manager.hpp"
PHOSPHOR_LOG2_USING;

#define SMBUS_RX_BUFFER_SIZE   2
#define STATE_CAUSE_SIZE       2

using Host = sdbusplus::common::xyz::openbmc_project::state::Host;

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
  kSLP_S0                                   = 0x00,
  kSLP_S3                                   = 0x03,
  kSLP_S4                                   = 0x04,
  kSLP_S5                                   = 0x05,
  kSLP_LP                                   = 0x06,
  kSLP_Unknown                              = 0xFF,
};

std::unordered_map<Host::HostState, uint8_t> hostStateToNative = {
    { Host::HostState::Running,  std::to_underlying(PowerState::kSLP_S0) },
    { Host::HostState::Standby,  std::to_underlying(PowerState::kSLP_S3) },
    { Host::HostState::Quiesced, std::to_underlying(PowerState::kSLP_S4) },
    { Host::HostState::Off,      std::to_underlying(PowerState::kSLP_S5) },
};

std::unordered_map<uint8_t, Host::HostState> nativeToHostState = {
    { std::to_underlying(PowerState::kSLP_S0), Host::HostState::Running  },
    { std::to_underlying(PowerState::kSLP_S3), Host::HostState::Standby  },
    { std::to_underlying(PowerState::kSLP_S4), Host::HostState::Quiesced },
    { std::to_underlying(PowerState::kSLP_S5), Host::HostState::Off      },
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
    kSMBUSOperationStatus_Undefined        = 0xFF,
};

enum class SMBUSCapability : uint8_t {
  kSmbusCapabilities_Unisolated            = 0,
  kSmbusCapabilities_Isolated              = 1,
  kSmbusCapabilities_Unknown               = 0xFF,
}; 

// Cache of important operational details
struct SequenceMcuContext {
    PowerEvent power_state_to_transmit;
    SMBUSCapability capabilities;
    TransitionCause last_known_transition_cause;
    PowerState last_known_power_state;
};

class SequenceMCUHandler {
    public:
        // https://google.github.io/styleguide/cppguide.html#Implicit_Conversions
        explicit SequenceMCUHandler(SMBUSManager smbus_init) :
            sequence_smbus_instance_(smbus_init),
            seq_mcu_ctx_(InitSequenceMcuContext()) {}

        ~SequenceMCUHandler() {}

        // NOTE: May reduce to one issue transition function with parameter if 
        // not needed
        SMBUSOperationStatus IssueAwakeCmd();
        SMBUSOperationStatus IssueSoftReset();
        SMBUSOperationStatus IssueHardReset();
        SMBUSOperationStatus IssueSoftShutdown();
        SMBUSOperationStatus IssueHardShutdown();

        SMBUSOperationStatus GetPowerState(Host::HostState& current_power_state);
        SMBUSOperationStatus GetTransitionCause(TransitionCause& transition_cause);
        SMBUSOperationStatus GetStateAndTransitionCause(std::pair<PowerState, TransitionCause>& gst_pair);
        SMBUSOperationStatus GetCapability(SMBUSCapability& get_capability);


    private:
        SMBUSManager sequence_smbus_instance_;
        SequenceMcuContext seq_mcu_ctx_;

        inline SequenceMcuContext InitSequenceMcuContext(void) {
            SequenceMcuContext init_return = {
                .power_state_to_transmit     = PowerEvent::kPowerEvent_Unknown,
                .capabilities                = SMBUSCapability::kSmbusCapabilities_Unknown,
                .last_known_transition_cause = TransitionCause::kTransitionCause_Unknown, 
                .last_known_power_state      = PowerState::kSLP_Unknown
            };

            return init_return;
        };
    };

#endif // SEQUENCE_MCU_HANDLER_HPP_
