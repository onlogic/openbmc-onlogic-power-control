#include "sequence_mcu_handler.hpp"

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
        std::to_underlying(CommandCode::GetTransitionCause), &output);
    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    if(!validCapabilities.contains(output)) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
    }

    get_capability = validCapabilities.at(output);

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}