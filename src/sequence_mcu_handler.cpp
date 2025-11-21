#include "sequence_mcu_handler.hpp"
#include <iostream>
// ---------------------------------------------------------------------------------------------- //

SequenceMCUHandler::SequenceMCUHandler(SMBUSManager& smbus_init) :
    sequence_smbus_instance_(smbus_init),
    seq_mcu_ctx_(InitSequenceMcuContext()),
    stop_dbus_refresh_(false) {}

SequenceMCUHandler::~SequenceMCUHandler() {
    stop_dbus_refresh_ = false;

    mcu_manager_worker_pool_.stop();
    mcu_manager_worker_pool_.join();
}

SMBUSOperationStatus SequenceMCUHandler::IssueAwakeCmd(uint8_t retries) {
    if (!retries) {
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }
    // primative smbus interface to issue awake command
    int operation_status;
    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
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

    std::this_thread::sleep_for(5000ms);

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
    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
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
    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
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

void SequenceMCUHandler::PollAction() {
    McuPowerState last_power_state = power_state;
    TransitionCause last_power_state = transition_cause;

    while (!stop_dbus_refresh_) {
        
        std::cout << std::to_underlying(power_state) << std::endl;
        std::cout << std::to_underlying(transition_cause) << std::endl;

        #ifdef CACHE_CHANGE_LOGIC
        if (last_power_state != power_state ||
            last_power_state != transition_cause) {
            
            for (const auto& event_listener: listener_handlers)
                if (event_listener)
                    event_listener();
            }

            last_power_state = power_state;
            last_power_state = transition_cause;
        #endif
        
        std::this_thread::sleep_for(1000ms);
    }
}

void SequenceMCUHandler::PollCacheAndDbusEventManagement(boost::asio::io_context& io) {
    if (stop_dbus_refresh_) {
        return;
    }

    info("Start State Poll Thread");
    boost::asio::post(io, [this]() {
        this->PollAction();
    });
    info("Stopping State Poll Thread Action");
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
