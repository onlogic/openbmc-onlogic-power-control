#include "sequence_mcu_handler.hpp"

using Host = SequenceMCUHandler::Host;
using Chassis = sdbusplus::common::xyz::openbmc_project::state::Chassis;

// ----------------------------------- Host State Mappings ----------------------------------- //

const std::unordered_map<Host::HostState, uint8_t> SequenceMCUHandler::hostStateToNative = {
    { Host::HostState::Running,  std::to_underlying(PowerState::kSLP_S0) },
    { Host::HostState::Standby,  std::to_underlying(PowerState::kSLP_S3) },
    { Host::HostState::Standby, std::to_underlying(PowerState::kSLP_S4)  },
    { Host::HostState::Off,      std::to_underlying(PowerState::kSLP_S5) },
};

const std::unordered_map<uint8_t, Host::HostState> SequenceMCUHandler::nativeToHostState = {
    { std::to_underlying(PowerState::kSLP_S0), Host::HostState::Running  },
    { std::to_underlying(PowerState::kSLP_S3), Host::HostState::Standby  },
    { std::to_underlying(PowerState::kSLP_S4), Host::HostState::Standby  },
    { std::to_underlying(PowerState::kSLP_S5), Host::HostState::Off      },
};

const std::unordered_map<uint8_t, SMBUSCapability> SequenceMCUHandler::validCapabilities = {
    {std::to_underlying(SMBUSCapability::kSmbusCapabilities_Unisolated), SMBUSCapability::kSmbusCapabilities_Unisolated},
    {std::to_underlying(SMBUSCapability::kSmbusCapabilities_Isolated),   SMBUSCapability::kSmbusCapabilities_Isolated},
    {std::to_underlying(SMBUSCapability::kSmbusCapabilities_Unknown),    SMBUSCapability::kSmbusCapabilities_Unknown}
};

const std::unordered_map<uint8_t, Host::RestartCause> SequenceMCUHandler::mapTransitionCauseIPMItoOBMC = {
    // Unknown or generic
    {std::to_underlying(TransitionCause::kTransitionCause_Unknown),                Host::RestartCause::Unknown},

    // Remote/BMC commands
    {std::to_underlying(TransitionCause::kTransitionCause_ChassisControl),         Host::RestartCause::RemoteCommand},
    {std::to_underlying(TransitionCause::kTransitionCause_BMCVirtualPowerButtonPress), Host::RestartCause::RemoteCommand},
    {std::to_underlying(TransitionCause::kTransitionCause_BMCVirtualResetPress),   Host::RestartCause::RemoteCommand},

    // Physical buttons
    {std::to_underlying(TransitionCause::kTransitionCause_ResetPushbutton),         Host::RestartCause::ResetButton},
    {std::to_underlying(TransitionCause::kTransitionCause_PowerUpPushbutton),       Host::RestartCause::PowerButton},
    {std::to_underlying(TransitionCause::kTransitionCause_IgnitionSoftOffTimer),    Host::RestartCause::PowerButton},
    {std::to_underlying(TransitionCause::kTransitionCause_IgnitionHardOffTimer),    Host::RestartCause::PowerButton},
    {std::to_underlying(TransitionCause::kTransitionCause_IgnitionLowVoltageTimer), Host::RestartCause::PowerButton},

    // Watchdog
    {std::to_underlying(TransitionCause::kTransitionCause_WatchdogExpiration),      Host::RestartCause::WatchdogTimer},

    // Power policy
    {std::to_underlying(TransitionCause::kTransitionCause_AcRestoreAlways),         Host::RestartCause::PowerPolicyAlwaysOn},
    {std::to_underlying(TransitionCause::kTransitionCause_AcRestorePrevious),       Host::RestartCause::PowerPolicyPreviousState},

    // Soft reset
    {std::to_underlying(TransitionCause::kTransitionCause_SoftReset),               Host::RestartCause::SoftReset},

    // Scheduled/RTC/Timer
    {std::to_underlying(TransitionCause::kTransitionCause_PowerUpRtc),              Host::RestartCause::ScheduledPowerOn},
    {std::to_underlying(TransitionCause::kTransitionCause_IgnitionStartupTimer),    Host::RestartCause::ScheduledPowerOn},

    // OEM/Other
    {std::to_underlying(TransitionCause::kTransitionCause_Oem),                     Host::RestartCause::Unknown},
    {std::to_underlying(TransitionCause::kTransitionCause_ResetPef),                Host::RestartCause::Unknown},
    {std::to_underlying(TransitionCause::kTransitionCause_PowerCyclePef),           Host::RestartCause::Unknown},
    {std::to_underlying(TransitionCause::kTransitionCause_BMCSetValidationError),   Host::RestartCause::Unknown},
    {std::to_underlying(TransitionCause::kTransitionCause_BMCSetEventError),        Host::RestartCause::Unknown}
};

// ----------------------------------- Chassis State Mappings ----------------------------------- //

const std::unordered_map<Chassis::Transition, uint8_t> SequenceMCUHandler::chassisTransitionToNative = {
    { Chassis::Transition::On,          std::to_underlying(PowerEvent::kPowerEvent_Awake)     },
    { Chassis::Transition::Off,         std::to_underlying(PowerEvent::kPowerEvent_HardOff)   },
    { Chassis::Transition::PowerCycle,  std::to_underlying(PowerEvent::kPowerEvent_HardReset) },
};

const std::unordered_map<uint8_t, Chassis::PowerState> SequenceMCUHandler::nativeToChassisState = {
    { std::to_underlying(PowerState::kSLP_S0),      Chassis::PowerState::On  },
    { std::to_underlying(PowerState::kSLP_S3),      Chassis::PowerState::On  },
    { std::to_underlying(PowerState::kSLP_S4),      Chassis::PowerState::On  },
    { std::to_underlying(PowerState::kSLP_S5),      Chassis::PowerState::Off },
    { std::to_underlying(PowerState::kSLP_Unknown), Chassis::PowerState::Off }
};

const std::unordered_map<Chassis::PowerState, uint8_t> SequenceMCUHandler::chassisStateToNative = {
    { Chassis::PowerState::On,  std::to_underlying(PowerState::kSLP_S0) },
    { Chassis::PowerState::Off, std::to_underlying(PowerState::kSLP_S5) },
    // Transitioning states map to their target state expectations or are ignored in static lookups
    { Chassis::PowerState::TransitioningToOn,  std::to_underlying(PowerState::kSLP_S0) },
    { Chassis::PowerState::TransitioningToOff, std::to_underlying(PowerState::kSLP_S5) },
};

const std::unordered_map<uint8_t, Chassis::PowerStatus> SequenceMCUHandler::mapTransitionCauseToChassisPowerStatus = {
    // Explicit Commands 
    {std::to_underlying(TransitionCause::kTransitionCause_ChassisControl),          Chassis::PowerStatus::Good},
    {std::to_underlying(TransitionCause::kTransitionCause_SoftReset),               Chassis::PowerStatus::Good},
    {std::to_underlying(TransitionCause::kTransitionCause_BMCVirtualPowerButtonPress), Chassis::PowerStatus::Good},
    {std::to_underlying(TransitionCause::kTransitionCause_BMCVirtualResetPress),    Chassis::PowerStatus::Good},

    // Physical Button Presses
    {std::to_underlying(TransitionCause::kTransitionCause_ResetPushbutton),         Chassis::PowerStatus::Good},
    {std::to_underlying(TransitionCause::kTransitionCause_PowerUpPushbutton),       Chassis::PowerStatus::Good},

    // power restoration
    {std::to_underlying(TransitionCause::kTransitionCause_AcRestoreAlways),         Chassis::PowerStatus::Good},
    {std::to_underlying(TransitionCause::kTransitionCause_AcRestorePrevious),       Chassis::PowerStatus::Good},

    // logical interruptions, these are good
    {std::to_underlying(TransitionCause::kTransitionCause_WatchdogExpiration),      Chassis::PowerStatus::Good},
    {std::to_underlying(TransitionCause::kTransitionCause_PowerUpRtc),              Chassis::PowerStatus::Good},
    {std::to_underlying(TransitionCause::kTransitionCause_IgnitionStartupTimer),    Chassis::PowerStatus::Good},
    {std::to_underlying(TransitionCause::kTransitionCause_IgnitionSoftOffTimer),    Chassis::PowerStatus::Good},
    {std::to_underlying(TransitionCause::kTransitionCause_IgnitionHardOffTimer),    Chassis::PowerStatus::Good},

    // unknown
    {std::to_underlying(TransitionCause::kTransitionCause_Unknown),                 Chassis::PowerStatus::Undefined},
    {std::to_underlying(TransitionCause::kTransitionCause_Oem),                     Chassis::PowerStatus::Undefined},
    {std::to_underlying(TransitionCause::kTransitionCause_ResetPef),                Chassis::PowerStatus::Undefined},
    {std::to_underlying(TransitionCause::kTransitionCause_PowerCyclePef),           Chassis::PowerStatus::Undefined},
    {std::to_underlying(TransitionCause::kTransitionCause_BMCSetValidationError),   Chassis::PowerStatus::Undefined},
    {std::to_underlying(TransitionCause::kTransitionCause_BMCSetEventError),        Chassis::PowerStatus::Undefined},
    {std::to_underlying(TransitionCause::kTransitionCause_IgnitionLowVoltageTimer), Chassis::PowerStatus::Undefined}
};

// ---------------------------------------------------------------------------------------------- //

SMBUSOperationStatus SequenceMCUHandler::IssueAwakeCmd(uint8_t retries) {
    if (!retries) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }
    // primative smbus interface to issue awake command
    int operation_status;
    for(uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status = 
            sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                    std::to_underlying(PowerEvent::kPowerEvent_Awake));
        if (operation_status < 0) {
            std::this_thread::sleep_for(1000ms);
            continue;
        } else {
            return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
        }
    }

    return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
}

SMBUSOperationStatus SequenceMCUHandler::IssueSoftReset(uint8_t retries) {
    if (!retries) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }

    SMBUSOperationStatus operation_status;
    operation_status = IssueSoftShutdown();
    if (operation_status != SMBUSOperationStatus::kSMBUSOperationStatus_Success)
        return operation_status;
    
    std::this_thread::sleep_for(1000ms);

    operation_status = IssueAwakeCmd();
    if (operation_status != SMBUSOperationStatus::kSMBUSOperationStatus_Success)
        return operation_status;
    
    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::IssueHardReset(uint8_t retries) {
    if (!retries) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }

    // primative smbus interface to issue Hard Reset command
    int operation_status;
    for(uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status = 
            sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                    std::to_underlying(PowerEvent::kPowerEvent_HardReset));
        if (operation_status < 0) {
            std::this_thread::sleep_for(1000ms);
            continue;
        } else {
            return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
        }
    }

    return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
}

SMBUSOperationStatus SequenceMCUHandler::IssueSoftShutdown(uint8_t retries) {
    if (!retries) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }

    int operation_status;
    for(uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status = 
            sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                    std::to_underlying(PowerEvent::kPowerEvent_SoftOff));
        if (operation_status < 0) {
            std::this_thread::sleep_for(1000ms);
            continue;
        }
    }

    return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
}

SMBUSOperationStatus SequenceMCUHandler::IssueHardShutdown(uint8_t retries) {
    if (!retries) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }

    int operation_status;
    for(uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status 
            = sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                      std::to_underlying(PowerEvent::kPowerEvent_HardOff));
        if (operation_status < 0) {
            std::this_thread::sleep_for(1000ms);
            continue;
        } else {
            return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
        }
    }

    return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
}

SMBUSOperationStatus SequenceMCUHandler::GetPowerState(Host::HostState& current_power_state) {
    uint8_t output;
    int operation_status;
    operation_status 
        = sequence_smbus_instance_.SmbusSubaddressReadByte(std::to_underlying(CommandCode::GetPowerstate), 
                                                           &output);
    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    // make sure resultant has host dbus-state equivalent
    if (!nativeToHostState.contains(output)) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }

    // get our target in host-dbus compatable form
    current_power_state = nativeToHostState.at(output);
    
    // update cache
    seq_mcu_ctx_.last_known_power_state = current_power_state;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetTransitionCause(Host::RestartCause& transition_cause) {
    uint8_t output;
    int operation_status;
    operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(
        std::to_underlying(CommandCode::GetTransitionCause), &output);

    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    if (!mapTransitionCauseIPMItoOBMC.contains(output)) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }

    transition_cause = mapTransitionCauseIPMItoOBMC.at(output);

    // update cache
    seq_mcu_ctx_.last_known_transition_cause = transition_cause;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetStateAndTransitionCause(std::pair<Host::HostState, Host::RestartCause>& gst_pair) {
    uint8_t buf[STATE_AND_TCAUSE_SIZE];
    size_t size_read;
    int operation_status = sequence_smbus_instance_.SmbusSubaddressReadByteBlock(
        std::to_underlying(CommandCode::GetPowerStateAndTransitionCause), 
        STATE_AND_TCAUSE_SIZE,
        buf, &size_read
    );

    if (operation_status == 0) {
        info("BLOCK READ (subaddress 0x02): size_read={SIZE}, " 
            "buf[0]={B0_DEC}, buf[1]={B1_DEC}",
            "SIZE", size_read,
            "B0_DEC", buf[0],
            "B1_DEC", buf[1]);
    } else {
        error("BLOCK READ failed: {STATUS}", "STATUS", operation_status);
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    // make sure resultant has host dbus-state equivalent
    if (!nativeToHostState.contains(buf[0])) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }

    gst_pair.first = nativeToHostState.at(buf[0]);

    if (!mapTransitionCauseIPMItoOBMC.contains(buf[1])) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }

    gst_pair.second = mapTransitionCauseIPMItoOBMC.at(buf[1]);

    // update cache
    seq_mcu_ctx_.last_known_power_state = gst_pair.first;
    seq_mcu_ctx_.last_known_transition_cause = gst_pair.second;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetChassisPowerState(Chassis::PowerState& current_power_state) {
    uint8_t output;
    int operation_status;
    operation_status 
        = sequence_smbus_instance_.SmbusSubaddressReadByte(std::to_underlying(CommandCode::GetPowerstate), 
                                                           &output);
    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    if (!nativeToChassisState.contains(output)) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }

    current_power_state = nativeToChassisState.at(output);
    
    // update cache
    seq_mcu_ctx_.last_known_chassis_power_state = current_power_state;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetChassisPowerStatus(Chassis::PowerStatus& power_status) {
    uint8_t output;
    int operation_status;
    operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(
        std::to_underlying(CommandCode::GetTransitionCause), &output);

    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    // Maps the raw 'Transition Cause' from the MCU to the 'Power Status'
    if (!mapTransitionCauseToChassisPowerStatus.contains(output)) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }

    power_status = mapTransitionCauseToChassisPowerStatus.at(output);

    // update cache
    seq_mcu_ctx_.last_known_chassis_power_status = power_status;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetChassisStateAndPowerStatus(std::pair<Chassis::PowerState, Chassis::PowerStatus>& state_status_pair) {
    uint8_t buf[STATE_AND_TCAUSE_SIZE];
    size_t size_read;
    int operation_status = sequence_smbus_instance_.SmbusSubaddressReadByteBlock(
        std::to_underlying(CommandCode::GetPowerStateAndTransitionCause), 
        STATE_AND_TCAUSE_SIZE,
        buf, &size_read
    );

    if (operation_status < 0) {
        error("BLOCK READ failed: {STATUS}", "STATUS", operation_status);
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    if (!nativeToChassisState.contains(buf[0])) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }
    state_status_pair.first = nativeToChassisState.at(buf[0]);

    // Map Transition Cause to Power Status
    if (!mapTransitionCauseToChassisPowerStatus.contains(buf[1])) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }
    state_status_pair.second = mapTransitionCauseToChassisPowerStatus.at(buf[1]);

    // update cache
    seq_mcu_ctx_.last_known_chassis_power_state = state_status_pair.first;
    seq_mcu_ctx_.last_known_chassis_power_status = state_status_pair.second;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetCapability(SMBUSCapability& get_capability) {
    uint8_t output;
    int operation_status;
    operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(
        std::to_underlying(CommandCode::GetCapabilities), &output);

    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    if(!validCapabilities.contains(output)) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }

    get_capability = validCapabilities.at(output);

    // update cache
    seq_mcu_ctx_.capabilities = get_capability;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}
