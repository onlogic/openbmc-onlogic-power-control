#include "sequence_mcu_handler.hpp"

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
    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status = IssueSoftShutdown();
        if (operation_status != SMBUSOperationStatus::kSMBUSOperationStatus_Success)
            return operation_status;
        std::this_thread::sleep_for(500ms);
    }

    std::this_thread::sleep_for(2000ms);

    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status = IssueAwakeCmd();
        if (operation_status != SMBUSOperationStatus::kSMBUSOperationStatus_Success)
            return operation_status;
        std::this_thread::sleep_for(500ms);
    }
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

void SequenceMCUHandler::RegisterNotification(void (*listener_handler)()) {
    if (listener_handler == nullptr) {
        return;
    }

    if (listener_handlers.size() >= MAX_EVENT_LISTENERS) {
        return;
    }

    listener_handlers.push_back(listener_handler);
}

void SequenceMCUHandler::PollCacheAndDbusEventManagement(boost::asio::io_context& io) {

}

SMBUSOperationStatus SequenceMCUHandler::GetMcuPowerState(McuPowerState& current_power_state) {
    uint8_t output;
    int operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(
        std::to_underlying(CommandCode::GetPowerstate), &output);

    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    current_power_state = static_cast<McuPowerState>(output);
    
    // Update Cache
    seq_mcu_ctx_.power_state = current_power_state;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetTransitionCause(TransitionCause& transition_cause) {
    uint8_t output;
    int operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(
        std::to_underlying(CommandCode::GetTransitionCause), &output);

    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    transition_cause = static_cast<TransitionCause>(output);

    seq_mcu_ctx_.transition_cause = transition_cause;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetCapability(SMBUSCapability& get_capability) {
    uint8_t output;
    int operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(
        std::to_underlying(CommandCode::GetCapabilities), &output);

    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    get_capability = static_cast<SMBUSCapability>(output);

    seq_mcu_ctx_.capabilities = get_capability;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetStateAndTransitionCause(std::pair<McuPowerState, TransitionCause>& state_cause_pair) {
    uint8_t buf[STATE_AND_TCAUSE_SIZE];
    size_t size_read;

    int operation_status = sequence_smbus_instance_.SmbusSubaddressReadByteBlock(
        std::to_underlying(CommandCode::GetPowerStateAndTransitionCause), 
        STATE_AND_TCAUSE_SIZE,
        buf, &size_read
    );

    if (operation_status < 0) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    McuPowerState raw_state = static_cast<McuPowerState>(buf[0]);
    TransitionCause raw_cause = static_cast<TransitionCause>(buf[1]);

    state_cause_pair.first = raw_state;
    state_cause_pair.second = raw_cause;

    seq_mcu_ctx_.power_state = raw_state;
    seq_mcu_ctx_.transition_cause = raw_cause;

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

// SMBUSOperationStatus SequenceMCUHandler::GetChassisPowerState(Chassis::PowerState& current_power_state) {
//     uint8_t output;
//     int operation_status;
//     operation_status 
//         = sequence_smbus_instance_.SmbusSubaddressReadByte(std::to_underlying(CommandCode::GetPowerstate), 
//                                                            &output);
//     if (operation_status < 0) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
//     }

//     if (!nativeToChassisState.contains(output)) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
//     }

//     current_power_state = nativeToChassisState.at(output);
    
//     // update cache
//     seq_mcu_ctx_.last_known_chassis_power_state = current_power_state;

//     return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
// }

// SMBUSOperationStatus SequenceMCUHandler::GetChassisPowerStatus(Chassis::PowerStatus& power_status) {
//     uint8_t output;
//     int operation_status;
//     operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(
//         std::to_underlying(CommandCode::GetTransitionCause), &output);

//     if (operation_status < 0) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
//     }

//     // Maps the raw 'Transition Cause' from the MCU to the 'Power Status'
//     if (!mapTransitionCauseToChassisPowerStatus.contains(output)) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
//     }

//     power_status = mapTransitionCauseToChassisPowerStatus.at(output);

//     // update cache
//     seq_mcu_ctx_.last_known_chassis_power_status = power_status;

//     return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
// }

// SMBUSOperationStatus SequenceMCUHandler::GetChassisStateAndPowerStatus(std::pair<Chassis::PowerState, Chassis::PowerStatus>& state_status_pair) {
//     uint8_t buf[STATE_AND_TCAUSE_SIZE];
//     size_t size_read;
//     int operation_status = sequence_smbus_instance_.SmbusSubaddressReadByteBlock(
//         std::to_underlying(CommandCode::GetPowerStateAndTransitionCause), 
//         STATE_AND_TCAUSE_SIZE,
//         buf, &size_read
//     );

//     if (operation_status < 0) {
//         error("BLOCK READ failed: {STATUS}", "STATUS", operation_status);
//         return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
//     }

//     if (!nativeToChassisState.contains(buf[0])) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
//     }
//     state_status_pair.first = nativeToChassisState.at(buf[0]);

//     // Map Transition Cause to Power Status
//     if (!mapTransitionCauseToChassisPowerStatus.contains(buf[1])) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
//     }
//     state_status_pair.second = mapTransitionCauseToChassisPowerStatus.at(buf[1]);

//     // update cache
//     seq_mcu_ctx_.last_known_chassis_power_state = state_status_pair.first;
//     seq_mcu_ctx_.last_known_chassis_power_status = state_status_pair.second;

//     return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
// }

// SMBUSOperationStatus SequenceMCUHandler::GetCapability(SMBUSCapability& get_capability) {
//     uint8_t output;
//     int operation_status;
//     operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(
//         std::to_underlying(CommandCode::GetCapabilities), &output);

//     if (operation_status < 0) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
//     }

//     if(!validCapabilities.contains(output)) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
//     }

//     get_capability = validCapabilities.at(output);

//     // update cache
//     seq_mcu_ctx_.capabilities = get_capability;

//     return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
// }



// SMBUSOperationStatus SequenceMCUHandler::GetPowerState(Host::HostState& current_power_state) {
//     uint8_t output;
//     int operation_status;
//     operation_status 
//         = sequence_smbus_instance_.SmbusSubaddressReadByte(std::to_underlying(CommandCode::GetPowerstate), 
//                                                            &output);
//     if (operation_status < 0) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
//     }

//     // make sure resultant has host dbus-state equivalent
//     if (!nativeToHostState.contains(output)) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
//     }

//     // get our target in host-dbus compatable form
//     current_power_state = nativeToHostState.at(output);
    
//     // update cache
//     seq_mcu_ctx_.last_known_power_state = current_power_state;

//     return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
// }

// SMBUSOperationStatus SequenceMCUHandler::GetTransitionCause(Host::RestartCause& transition_cause) {
//     uint8_t output;
//     int operation_status;
//     operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(
//         std::to_underlying(CommandCode::GetTransitionCause), &output);

//     if (operation_status < 0) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
//     }

//     if (!mapTransitionCauseIPMItoOBMC.contains(output)) {
//         return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidCommand;
//     }

//     transition_cause = mapTransitionCauseIPMItoOBMC.at(output);

//     // update cache
//     seq_mcu_ctx_.last_known_transition_cause = transition_cause;

//     return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
// }