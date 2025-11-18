#include "sequence_mcu_handler.hpp"

using Host = SequenceMCUHandler::Host;

const std::unordered_map<Host::HostState, uint8_t> SequenceMCUHandler::hostStateToNative = {
    { Host::HostState::Running,  std::to_underlying(PowerState::kSLP_S0) },
    { Host::HostState::Standby,  std::to_underlying(PowerState::kSLP_S3) },
    { Host::HostState::Quiesced, std::to_underlying(PowerState::kSLP_S4) },
    { Host::HostState::Off,      std::to_underlying(PowerState::kSLP_S5) },
};

const std::unordered_map<uint8_t, Host::HostState> SequenceMCUHandler::nativeToHostState = {
    { std::to_underlying(PowerState::kSLP_S0), Host::HostState::Running  },
    { std::to_underlying(PowerState::kSLP_S3), Host::HostState::Standby  },
    { std::to_underlying(PowerState::kSLP_S4), Host::HostState::Quiesced },
    { std::to_underlying(PowerState::kSLP_S5), Host::HostState::Off      },
};

const std::unordered_map<uint8_t, SMBUSCapability> SequenceMCUHandler::validCapabilities = {
    {std::to_underlying(SMBUSCapability::kSmbusCapabilities_Unisolated), SMBUSCapability::kSmbusCapabilities_Unisolated},
    {std::to_underlying(SMBUSCapability::kSmbusCapabilities_Isolated),   SMBUSCapability::kSmbusCapabilities_Isolated},
    {std::to_underlying(SMBUSCapability::kSmbusCapabilities_Unknown),    SMBUSCapability::kSmbusCapabilities_Unknown}
};

const std::unordered_map<uint8_t, Host::RestartCause> SequenceMCUHandler::mapTransitionCauseIPMItoOBMC = {
    // Unknown or generic
    {std::to_underlying(TransitionCause::kTransitionCause_Unknown),               Host::RestartCause::Unknown},

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
    {std::to_underlying(TransitionCause::kTransitionCause_WatchdogExpiration),     Host::RestartCause::WatchdogTimer},

    // Power policy
    {std::to_underlying(TransitionCause::kTransitionCause_AcRestoreAlways),        Host::RestartCause::PowerPolicyAlwaysOn},
    {std::to_underlying(TransitionCause::kTransitionCause_AcRestorePrevious),      Host::RestartCause::PowerPolicyPreviousState},

    // Soft reset
    {std::to_underlying(TransitionCause::kTransitionCause_SoftReset),              Host::RestartCause::SoftReset},

    // Scheduled/RTC/Timer
    {std::to_underlying(TransitionCause::kTransitionCause_PowerUpRtc),             Host::RestartCause::ScheduledPowerOn},
    {std::to_underlying(TransitionCause::kTransitionCause_IgnitionStartupTimer),   Host::RestartCause::ScheduledPowerOn},

    // Host crash
    // Nothing yet

    // OEM/Other
    {std::to_underlying(TransitionCause::kTransitionCause_Oem),                     Host::RestartCause::Unknown},
    {std::to_underlying(TransitionCause::kTransitionCause_ResetPef),                Host::RestartCause::Unknown},
    {std::to_underlying(TransitionCause::kTransitionCause_PowerCyclePef),           Host::RestartCause::Unknown},
    {std::to_underlying(TransitionCause::kTransitionCause_BMCSetValidationError),   Host::RestartCause::Unknown},
    {std::to_underlying(TransitionCause::kTransitionCause_BMCSetEventError),        Host::RestartCause::Unknown}
};

SMBUSOperationStatus SequenceMCUHandler::IssueAwakeCmd() {
    // primative smbus interface to issue Hard Reset command
    int operation_status;
    operation_status = sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                               std::to_underlying(PowerEvent::kPowerEvent_Awake));
    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::IssueSoftReset() {
    SMBUSOperationStatus operation_status;

    operation_status = IssueSoftShutdown();
    if (operation_status != SMBUSOperationStatus::kSMBUSOperationStatus_Success)
        return operation_status;

    operation_status = IssueAwakeCmd();
    if (operation_status != SMBUSOperationStatus::kSMBUSOperationStatus_Success)
        return operation_status;
    
    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::IssueHardReset() {
    // primative smbus interface to issue Hard Reset command
    int operation_status;
    operation_status = sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                               std::to_underlying(PowerEvent::kPowerEvent_HardReset));
    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::IssueSoftShutdown() {
    // primative smbus interface to issue soft shutdown command
    int operation_status;
    operation_status = sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                               std::to_underlying(PowerEvent::kPowerEvent_SoftOff));
    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::IssueHardShutdown() {
    // primative smbus interface to issue hard shutdown command
    int operation_status;
    operation_status = sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                               std::to_underlying(PowerEvent::kPowerEvent_HardOff));
    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetPowerState(Host::HostState& current_power_state) {
    uint8_t output;
    int operation_status;
    operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(std::to_underlying(CommandCode::GetPowerstate), 
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

    // TODO correctly map transition causes from IPMI to custom OBMC interface
    if (!mapTransitionCauseIPMItoOBMC.contains(buf[1])) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }

    gst_pair.second = mapTransitionCauseIPMItoOBMC.at(buf[1]);

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

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}