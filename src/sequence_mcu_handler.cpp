#include "sequence_mcu_handler.hpp"

SmbusOperationStatus SequenceMCUHandler::IssueAwakeCmd(){

    return SmbusOperationStatus::kSmbusOperationStatus_Success;
}

SmbusOperationStatus SequenceMCUHandler::IssueSoftReset(){

    return SmbusOperationStatus::kSmbusOperationStatus_Success;
}

SmbusOperationStatus SequenceMCUHandler::IssueHardReset(){

    return SmbusOperationStatus::kSmbusOperationStatus_Success;
}

SmbusOperationStatus SequenceMCUHandler::IssueSoftShutdown(){

    return SmbusOperationStatus::kSmbusOperationStatus_Success;
}

SmbusOperationStatus SequenceMCUHandler::IssueHardShutdown(){

    return SmbusOperationStatus::kSmbusOperationStatus_Success;
}

SmbusOperationStatus SequenceMCUHandler::GetPowerState(PowerState& current_power_state){

    return SmbusOperationStatus::kSmbusOperationStatus_Success;
}

SmbusOperationStatus SequenceMCUHandler::GetTransitionCause(TransitionCause& transition_cause){

    return SmbusOperationStatus::kSmbusOperationStatus_Success;
}

SmbusOperationStatus SequenceMCUHandler::GetStateAndTransitionCause(std::pair<PowerState, TransitionCause>& gst_pair) {

    return SmbusOperationStatus::kSmbusOperationStatus_Success;
}

SmbusOperationStatus SequenceMCUHandler::GetCapability(SMBUSCapability& get_capability){

    return SmbusOperationStatus::kSmbusOperationStatus_Success;
}