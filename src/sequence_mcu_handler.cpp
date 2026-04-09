// SPDX-FileCopyrightText: (c) 2026 OnLogic, Inc. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
// SPDX-FileContributor: Justin Seely <justin.seely@onlogic.com>
// SPDX-FielContributor: Nicholas Hanna <nick.hanna@onlogic.com>  (SMBUS command set, MCU endpoint layer, MCU handler API, power control integration into DBus API, project structure)
#include "sequence_mcu_handler.hpp"
#include <iostream>

SequenceMCUHandler::SequenceMCUHandler(SMBUSManager& smbus_init,
                                       boost::asio::io_context& io) :
    sequence_smbus_instance_(smbus_init),
    current_state(InitSequenceMcuContext()),
    poll_timer_(io, std::chrono::milliseconds(500)),
    stop_dbus_refresh_(false)
    {}

SequenceMCUHandler::~SequenceMCUHandler() {
    stop_dbus_refresh_ = true;
    poll_timer_.cancel();
}

SMBUSOperationStatus SequenceMCUHandler::IssueAwakeCmd(uint8_t retries) {
    info("SequenceMCUHandler :: IssueAwakeCmd started. Retries: {RETRIES}", "RETRIES", retries);

    if (!retries) {
        error("SequenceMCUHandler :: IssueAwakeCmd failed: Invalid retries: {RETRIES}", "RETRIES", retries);
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }
    // use primative smbus interface to issue awake command
    int operation_status;
    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status = 
            sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                    std::to_underlying(PowerEvent::kPowerEvent_Awake));
        if (operation_status < 0) {
            error("SequenceMCUHandler :: IssueAwakeCmd failed: Retry Attempt: {ATTEMPT}", "ATTEMPT", attempt + 1);
            std::this_thread::sleep_for(1000ms);
            continue;
        } else {
            info("SequenceMCUHandler :: IssueAwakeCmd command sent.");
            return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
        }
    }

    info("SequenceMCUHandler :: IssueAwakeCmd command Failed.");
    return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
}

SMBUSOperationStatus SequenceMCUHandler::IssueSoftReset(uint8_t retries) {
    info("SequenceMCUHandler :: IssueSoftReset started. Retries: {RETRIES}", "RETRIES", retries);

    if (!retries) {
        error("SequenceMCUHandler :: IssueSoftReset failed: Invalid retries: {RETRIES}", "RETRIES", retries);
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }

    SMBUSOperationStatus operation_status;
    debug("SequenceMCUHandler :: Beginning Shutdown");
    // Soft Shutdown
    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status = IssueSoftShutdown();
        
        if (operation_status != SMBUSOperationStatus::kSMBUSOperationStatus_Success) {
            error("SequenceMCUHandler :: SoftShutdown failed on attempt {ATTEMPT}. Status: {STATUS}", 
                  "ATTEMPT", attempt + 1, 
                  "STATUS", static_cast<int>(operation_status));
            return operation_status;
        }
        std::this_thread::sleep_for(500ms);
    }

    info("SequenceMCUHandler :: SoftShutdown commands sent. Waiting 5000ms for device reset.");
    std::this_thread::sleep_for(5000ms);

    // Awake
    debug("SequenceMCUHandler :: Beginning Awake");
    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status = IssueAwakeCmd();
        
        if (operation_status != SMBUSOperationStatus::kSMBUSOperationStatus_Success) {
            error("SequenceMCUHandler :: AwakeCmd failed on attempt {ATTEMPT}. Status: {STATUS}", 
                  "ATTEMPT", attempt + 1, 
                  "STATUS", static_cast<int>(operation_status));
            return operation_status;
        }
        std::this_thread::sleep_for(500ms);
    }

    info("SequenceMCUHandler :: IssueSoftReset completed");
    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::IssueHardReset(uint8_t retries) {
    info("SequenceMCUHandler :: IssueHardReset started. Retries: {RETRIES}", "RETRIES", retries);

    if (!retries) {
        error("SequenceMCUHandler :: IssueHardReset failed: Invalid retries: {RETRIES}", "RETRIES", retries);
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }

    // primative smbus interface to issue Hard Reset command
    int operation_status;
    for(uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status = 
            sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                    std::to_underlying(PowerEvent::kPowerEvent_HardReset));
        if (operation_status < 0) {
            error("SequenceMCUHandler :: IssueHardReset failed: Retry Attempt: {ATTEMPT}", "ATTEMPT", attempt + 1);
            std::this_thread::sleep_for(1000ms);
            continue;
        } else {
            info("SequenceMCUHandler :: IssueHardReset command sent.");
            return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
        }
    }

    info("SequenceMCUHandler :: IssueHardReset command Failed.");
    return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
}

SMBUSOperationStatus SequenceMCUHandler::IssueSoftShutdown(uint8_t retries) {
    info("SequenceMCUHandler :: IssueSoftShutdown started. Retries: {RETRIES}", "RETRIES", retries);

    if (!retries) {
        error("SequenceMCUHandler :: IssueSoftShutdown failed: Invalid retries: {RETRIES}", "RETRIES", retries);
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }

    int operation_status;
    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status 
            = sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState),
                                                      std::to_underlying(PowerEvent::kPowerEvent_SoftOff));
        if (operation_status < 0) {
            error("SequenceMCUHandler :: IssueSoftShutdown failed: Retry Attempt: {ATTEMPT}", "ATTEMPT", attempt + 1);
            std::this_thread::sleep_for(1000ms);
            continue;
        } else {
            debug("SequenceMCUHandler :: IssueSoftShutdown command sent.");
            return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
        }
    }

    error("SequenceMCUHandler :: IssueSoftShutdown command Failed.");
    return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
}

SMBUSOperationStatus SequenceMCUHandler::IssueHardShutdown(uint8_t retries) {
    info("SequenceMCUHandler :: IssueHardShutdown started. Retries: {RETRIES}", "RETRIES", retries);

    if (!retries) {
        error("SequenceMCUHandler :: IssueHardShutdown failed: Invalid retries: {RETRIES}", "RETRIES", retries);
        return SMBUSOperationStatus::kSMBUSOperationStatus_InvalidRetries;
    }

    int operation_status;
    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
        operation_status 
            = sequence_smbus_instance_.SmbusWriteByte(std::to_underlying(CommandCode::SetPowerState), 
                                                      std::to_underlying(PowerEvent::kPowerEvent_HardOff));
        if (operation_status < 0) {
            error("SequenceMCUHandler :: IssueHardShutdown failed: Retry Attempt: {ATTEMPT}", "ATTEMPT", attempt + 1);
            std::this_thread::sleep_for(1000ms);
            continue;
        } else {
            debug("SequenceMCUHandler :: IssueHardShutdown command sent.");
            return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
        }
    }

    error("SequenceMCUHandler :: IssueHardShutdown command Failed.");
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

#ifdef USE_COROUTINE
// Keep this for reference in case async recursive callback does not work.
// TBH, I'd recommend doing it like this, more standard, but the BMC environment is rejecting it.
void SequenceMCUHandler::StartPolling() {

    std::cerr << "START POLLING CALLED" << std::endl;

    boost::asio::spawn(poll_timer_.get_executor(), [this](boost::asio::yield_context yield) {

        std::cerr << "CREATED COROUTINE" << std::endl;

        #ifdef CACHE_CHANGE_LOGIC
        McuPowerState last_power_state = seq_mcu_ctx_.power_state;
        TransitionCause last_cause = seq_mcu_ctx_.transition_cause;
        #endif

        while (!this->stop_dbus_refresh_) {

            std::cerr << "Poll Loop Power State: " 
                      << static_cast<int>(std::to_underlying(seq_mcu_ctx_.power_state)) 
                      << std::endl;

            std::cerr << "Poll Loop Transition Cause: " 
                      << static_cast<int>(std::to_underlying(seq_mcu_ctx_.transition_cause)) 
                      << std::endl;

            #ifdef CACHE_CHANGE_LOGIC
            if (last_power_state != seq_mcu_ctx_.power_state || 
                last_cause != seq_mcu_ctx_.transition_cause) {
                
                for (const auto& event_listener : this->listener_handlers) {
                    if (event_listener) event_listener();
                }

                last_power_state = seq_mcu_ctx_.power_state;
                last_cause = seq_mcu_ctx_.transition_cause;
            }
            #endif

            this->poll_timer_.expires_after(std::chrono::seconds(1));

            boost::system::error_code ec;
            this->poll_timer_.async_wait(yield[ec]);

            // If cancelled, break loop
            if (ec == boost::asio::error::operation_aborted) break;
        }
    });
}
#endif

void SequenceMCUHandler::StartPolling() {
    // Reschedule the timer for the next poll
    poll_timer_.expires_after(std::chrono::milliseconds(500));

    poll_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted || stop_dbus_refresh_) {
            info("SequenceMCUHandler :: Poll timer aborted");
            return;
        } else if (ec) {
            error("SequenceMCUHandler :: Poll timer error, ending: {ERROR}", "ERROR", ec.message());
            return;
        }

        try {
            // Perform the polling operation
            CacheSystemState();
        } catch (const std::exception& e) {
            error("SequenceMCUHandler :: System state caching failed, will retry on next poll: {ERROR}", "ERROR", e.what());
        }

        // Recursive call to continue polling
        StartPolling();
    });
}

void SequenceMCUHandler::CacheSystemState() {
    // Update current state using cached variable
    std::pair<McuPowerState, TransitionCause> state_update = {};
    auto status = GetStateAndTransitionCause(state_update);
    if (status != SMBUSOperationStatus::kSMBUSOperationStatus_Success) {
        error("SequenceMCUHandler :: PollSystemState failed to get state and cause: {STATUS}", "STATUS", static_cast<int>(status));
        return;
    }

    if (current_state.power_state != state_update.first) {
        info("SequenceMCUHandler :: State change detected. New State: {NEW_STATE}, Old State: {OLD_STATE}", 
            "NEW_STATE", static_cast<int>(std::to_underlying(state_update.first)), 
            "OLD_STATE", static_cast<int>(std::to_underlying(current_state.power_state)));
        current_state.power_state = state_update.first;
    }

    if (current_state.transition_cause != state_update.second) {
        info("SequenceMCUHandler :: Transition Cause change detected. New Cause: {NEW_CAUSE}, Old Cause: {OLD_CAUSE}", 
            "NEW_CAUSE", static_cast<int>(std::to_underlying(state_update.second)), 
            "OLD_CAUSE", static_cast<int>(std::to_underlying(current_state.transition_cause)));
        current_state.transition_cause = state_update.second;
    }

    #ifdef CACHE_CHANGE_LOGIC
    if (last_state != current_state || last_cause != current_cause) {
        for (const auto& event_listener : this->listener_handlers) {
            if (event_listener) event_listener();
        }
        // Update history to match current for next iteration
        last_state = current_state;
        last_cause = current_cause;
    }
    #endif
}

SMBUSOperationStatus SequenceMCUHandler::GetMcuPowerState(McuPowerState& current_power_state) {
    current_power_state = current_state.power_state;
    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetTransitionCause(TransitionCause& transition_cause) {
    transition_cause = current_state.transition_cause;
    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetCapability(SMBUSCapability& get_capability) {
    debug("SequenceMCUHandler :: GetCapability called");
    uint8_t output;
    int operation_status = sequence_smbus_instance_.SmbusSubaddressReadByte(
        std::to_underlying(CommandCode::GetCapabilities), &output);

    if (operation_status < 0) {
        error("SequenceMCUHandler :: GetCapability failed: Protocol error");
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    get_capability = static_cast<SMBUSCapability>(output);

    // Update Cache
    current_state.capabilities = get_capability;

    debug("SequenceMCUHandler :: GetCapability success. Capability: {CAP}", "CAP", static_cast<int>(get_capability));

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}

SMBUSOperationStatus SequenceMCUHandler::GetStateAndTransitionCause(std::pair<McuPowerState, TransitionCause>& state_cause_pair) {
    debug("SequenceMCUHandler :: GetStateAndTransitionCause called");
    uint8_t buf[STATE_AND_TCAUSE_SIZE];
    size_t size_read;

    int operation_status = sequence_smbus_instance_.SmbusSubaddressReadByteBlock(
        std::to_underlying(CommandCode::GetPowerStateAndTransitionCause), 
        STATE_AND_TCAUSE_SIZE,
        buf, &size_read
    );

    if (operation_status < 0) {
        error("SequenceMCUHandler :: GetStateAndTransitionCause failed: Protocol error");
        return SMBUSOperationStatus::kSMBUSOperationStatus_ProtocolError;
    }

    state_cause_pair.first = static_cast<McuPowerState>(buf[0]);
    state_cause_pair.second = static_cast<TransitionCause>(buf[1]);

    debug("SequenceMCUHandler :: GetStateAndTransitionCause success. State: {STATE}, Cause: {CAUSE}",
        "STATE", static_cast<int>(state_cause_pair.first),
        "CAUSE", static_cast<int>(state_cause_pair.second));

    return SMBUSOperationStatus::kSMBUSOperationStatus_Success;
}
