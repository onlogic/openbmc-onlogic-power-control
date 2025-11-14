#include "sequence_mcu_handler.hpp"

OperationStatus SequenceMCUHandler::IssueAwakeCmd(){

    return OperationStatus::kOperationStatus_Success;
}

OperationStatus SequenceMCUHandler::IssueSoftReset(){

    return OperationStatus::kOperationStatus_Success;
}

OperationStatus SequenceMCUHandler::IssueHardReset(){

    return OperationStatus::kOperationStatus_Success;
}

OperationStatus SequenceMCUHandler::IssueSoftShutdown(){

    return OperationStatus::kOperationStatus_Success;
}

OperationStatus SequenceMCUHandler::IssueHardShutdown(){

    return OperationStatus::kOperationStatus_Success;
}

OperationStatus SequenceMCUHandler::GetPowerState(PowerState& current_power_state){

    return OperationStatus::kOperationStatus_Success;
}

OperationStatus SequenceMCUHandler::GetTransitionCause(TransitionCause& transition_cause){

    return OperationStatus::kOperationStatus_Success;
}

OperationStatus SequenceMCUHandler::GetStateAndTransitionCause(std::pair<PowerState, TransitionCause>& gst_pair) {

    return OperationStatus::kOperationStatus_Success;
}

OperationStatus SequenceMCUHandler::GetCapability(SMBUSCapability& get_capability){

    return OperationStatus::kOperationStatus_Success;
}