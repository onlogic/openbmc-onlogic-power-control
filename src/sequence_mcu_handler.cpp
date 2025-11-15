#include "sequence_mcu_handler.hpp"

SMBUSOperationStatus SequenceMCUHandler::IssueAwakeCmd() {

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::IssueSoftReset() {

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::IssueHardReset() {

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::IssueSoftShutdown() {

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::IssueHardShutdown() {

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetPowerState(Host::HostState& current_power_state){
    // primative smbus interface to get state
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
    current_power_state = nativeToHostState[output];
    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetTransitionCause(TransitionCause& transition_cause) {
    
    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetStateAndTransitionCause(std::pair<PowerState, TransitionCause>& gst_pair) {

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetCapability(SMBUSCapability& get_capability) {

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}