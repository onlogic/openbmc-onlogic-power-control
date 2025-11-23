#include "chassis.hpp"
#include "utils.hpp"
#include <phosphor-logging/lg2.hpp>
#include "sequence_mcu_handler.hpp" 

PHOSPHOR_LOG2_USING;

// ----------------------------------- Chassis State Mappings ----------------------------------- //
const std::unordered_map<Chassis::Transition, uint8_t> Chassis::chassisTransitionToNative = {
    { Chassis::Transition::On,          std::to_underlying(PowerEvent::kPowerEvent_Awake)     },
    { Chassis::Transition::Off,         std::to_underlying(PowerEvent::kPowerEvent_HardOff)   },
    { Chassis::Transition::PowerCycle,  std::to_underlying(PowerEvent::kPowerEvent_HardReset) },
};

const std::unordered_map<uint8_t, Chassis::PowerState> Chassis::nativeToChassisState = {
    { std::to_underlying(McuPowerState::kSLP_S0),      Chassis::PowerState::On  },
    { std::to_underlying(McuPowerState::kSLP_S3),      Chassis::PowerState::On  },
    { std::to_underlying(McuPowerState::kSLP_S4),      Chassis::PowerState::On  },
    { std::to_underlying(McuPowerState::kSLP_S5),      Chassis::PowerState::Off },
    { std::to_underlying(McuPowerState::kSLP_LP),      Chassis::PowerState::Off },
    { std::to_underlying(McuPowerState::kSLP_Unknown), Chassis::PowerState::Off }
};

const std::unordered_map<Chassis::PowerState, uint8_t> Chassis::chassisStateToNative = {
    { Chassis::PowerState::On,  std::to_underlying(McuPowerState::kSLP_S0) },
    { Chassis::PowerState::Off, std::to_underlying(McuPowerState::kSLP_S5) },
    // Transitioning states map to their target state expectations or are ignored in static lookups
    { Chassis::PowerState::TransitioningToOn,  std::to_underlying(McuPowerState::kSLP_S0) },
    { Chassis::PowerState::TransitioningToOff, std::to_underlying(McuPowerState::kSLP_S5) },
};

const std::unordered_map<uint8_t, Chassis::PowerStatus> Chassis::mapTransitionCauseToChassisPowerStatus = {
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

// map Raw PowerState to DBus PowerState
static Chassis::PowerState toChassisState(McuPowerState raw) {
    auto key = std::to_underlying(raw);
    if (Chassis::nativeToChassisState.contains(key)) {
        return Chassis::nativeToChassisState.at(key);
    }
    return Chassis::PowerState::Off;
}

// map Raw TransitionCause to DBus PowerStatus
static Chassis::PowerStatus toPowerStatus(TransitionCause raw) {
    auto key = std::to_underlying(raw);
    if (Chassis::mapTransitionCauseToChassisPowerStatus.contains(key)) {
        return Chassis::mapTransitionCauseToChassisPowerStatus.at(key);
    }
    return Chassis::PowerStatus::Undefined;
}

Chassis::Chassis(std::shared_ptr<sdbusplus::asio::connection> conn, const std::string& node, SequenceMCUHandler& seq_mcu_comm_handler)
    : ObjectServer(*conn, getPath(node).c_str(), Chassis::default_service, node), node(node), seq_mcu_comm_handler_(seq_mcu_comm_handler)
{
    // TODO
    // Register Event listener here

    determineInitialState();
}

std::string Chassis::getPath(const std::string& node)
{
    return std::string(Chassis::namespace_path::value) + "/" + std::string(Chassis::namespace_path::chassis) + node;
}

Chassis::Transition Chassis::requestedPowerTransition(Transition value)
{
    // info("Chassis{NODE}: Requested power transition from {OLD} to {NEW}", "NODE", node, "OLD", sdbusplus::server::xyz::openbmc_project::state::Chassis::requestedPowerTransition(), "NEW", value);
    switch (value) {
        case (Chassis::Transition::On) : {
            // smbus_.SmbusWriteByte(0x04, 0x00);
            seq_mcu_comm_handler_.IssueAwakeCmd();
            info("Awake Signal written");
            break;
        } case (Chassis::Transition::Off) : {
            // smbus_.SmbusWriteByte(0x04, 0x03);
            seq_mcu_comm_handler_.IssueHardShutdown();
            info("Hard Off Signal written");
            break;
        }
        case (Chassis::Transition::PowerCycle) : {
            seq_mcu_comm_handler_.IssueHardReset();
            break;
        }
        default : {
            error("Unhandled Request");
            break;
        }
    }

    return sdbusplus::server::xyz::openbmc_project::state::Chassis::requestedPowerTransition(value);
}

Chassis::Transition Chassis::requestedPowerTransition() const
{
    return sdbusplus::server::xyz::openbmc_project::state::Chassis::requestedPowerTransition();
}

Chassis::PowerState Chassis::currentPowerState(PowerState value)
{
    info("Chassis{NODE}: Current power state change to {NEW}", "NODE", node, "NEW", value);
    sdbusplus::server::xyz::openbmc_project::state::Chassis::currentPowerState(value);
    lastStateChangeTime(getCurrentTimeMs());
    return value;
}

Chassis::PowerState Chassis::currentPowerState() const
{
    McuPowerState raw_state;
    auto status = seq_mcu_comm_handler_.GetMcuPowerState(raw_state);
    
    if (status != SMBUSOperationStatus::kSMBUSOperationStatus_Success) {
        error("GET state failed: {STATUS}", "STATUS", std::to_underlying(status));
        return sdbusplus::server::xyz::openbmc_project::state::Chassis::currentPowerState();
    }
    
    return toChassisState(raw_state);
}

Chassis::PowerStatus Chassis::currentPowerStatus(PowerStatus value)
{
    // Noop hook: add logic here if needed        
    info("Chassis{NODE}: Current power status change to {NEW}", "NODE", node, "NEW", value);
    sdbusplus::server::xyz::openbmc_project::state::Chassis::currentPowerStatus(value);
    lastStateChangeTime(getCurrentTimeMs());
    return value;
}

Chassis::PowerStatus Chassis::currentPowerStatus() const
{
    return sdbusplus::server::xyz::openbmc_project::state::Chassis::currentPowerStatus();
}

void Chassis::determineInitialState()
{
    std::pair<McuPowerState, TransitionCause> raw_state_cause;
    auto status = seq_mcu_comm_handler_.GetStateAndTransitionCause(raw_state_cause);

    info("GetChassisStateAndPowerStatus: {STATUS}, raw_state: {STATE}, raw_cause: {CAUSE}",
        "STATUS", std::to_underlying(status),
        "STATE", std::to_underlying(raw_state_cause.first),
        "CAUSE", std::to_underlying(raw_state_cause.second));

    if (status != SMBUSOperationStatus::kSMBUSOperationStatus_Success) {
        lastStateChangeTime(getCurrentTimeMs());
        return;
    }

    Chassis::PowerState mapped_state = toChassisState(raw_state_cause.first);
    currentPowerState(mapped_state);

    Chassis::PowerStatus mapped_status = toPowerStatus(raw_state_cause.second);
    currentPowerStatus(mapped_status);

    Chassis::Transition type_to_init;
    if (mapped_state == Chassis::PowerState::On) {
        type_to_init = Chassis::Transition::On;
    }
    else {
        type_to_init = Chassis::Transition::Off;
    }

    // or requestedPowerTransition(type_to_init);?
    sdbusplus::server::xyz::openbmc_project::state::Chassis::requestedPowerTransition(type_to_init);
    lastStateChangeTime(getCurrentTimeMs());
}
