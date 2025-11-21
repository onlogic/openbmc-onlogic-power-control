#include "host.hpp"
#include "utils.hpp"
#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

// ----------------------------------- Host State Mappings ----------------------------------- //
const std::unordered_map<McuPowerState, Host::HostState> Host::nativeToHostState = {
    {McuPowerState::kSLP_S0,        Host::HostState::Running},
    {McuPowerState::kSLP_S3,        Host::HostState::Standby}, 
    {McuPowerState::kSLP_S4,        Host::HostState::Standby},
    {McuPowerState::kSLP_S5,        Host::HostState::Off},
    {McuPowerState::kSLP_LP,        Host::HostState::Off},
    {McuPowerState::kSLP_Unknown,   Host::HostState::Off}
};

const std::unordered_map<TransitionCause, Host::RestartCause> Host::mapTransitionCauseToRestartCause = {
    {TransitionCause::kTransitionCause_ChassisControl,         Host::RestartCause::RemoteCommand},
    {TransitionCause::kTransitionCause_ResetPushbutton,        Host::RestartCause::ResetButton},
    {TransitionCause::kTransitionCause_PowerUpPushbutton,      Host::RestartCause::PowerButton},
    {TransitionCause::kTransitionCause_WatchdogExpiration,     Host::RestartCause::WatchdogTimer},
    {TransitionCause::kTransitionCause_SoftReset,              Host::RestartCause::SoftReset},
    {TransitionCause::kTransitionCause_PowerUpRtc,             Host::RestartCause::ScheduledPowerOn},
    {TransitionCause::kTransitionCause_AcRestoreAlways,        Host::RestartCause::PowerPolicyAlwaysOn},
    {TransitionCause::kTransitionCause_AcRestorePrevious,      Host::RestartCause::PowerPolicyPreviousState},
    {TransitionCause::kTransitionCause_IgnitionStartupTimer,   Host::RestartCause::ScheduledPowerOn},
    {TransitionCause::kTransitionCause_BMCVirtualPowerButtonPress, Host::RestartCause::PowerButton},
    {TransitionCause::kTransitionCause_BMCVirtualResetPress,   Host::RestartCause::ResetButton},
    {TransitionCause::kTransitionCause_Unknown,                Host::RestartCause::Unknown}
};

// Raw PowerState to DBus HostState
static Host::HostState toHostState(McuPowerState raw) {
    if (Host::nativeToHostState.contains(raw)) {
        return Host::nativeToHostState.at(raw);
    }
    error("Unknown raw power state: {RAW}", "RAW", std::to_underlying(raw));
    return Host::HostState::Off; // Default safe fallback
}

// TransitionCause To DBus RestartCause
static Host::RestartCause toRestartCause(TransitionCause raw) {
    if (Host::mapTransitionCauseToRestartCause.contains(raw)) {
        return Host::mapTransitionCauseToRestartCause.at(raw);
    }
    return Host::RestartCause::Unknown; 
}

Host::Host(std::shared_ptr<sdbusplus::asio::connection> conn, const std::string& node, SequenceMCUHandler& seq_mcu_comm_handler)
    : ObjectServer(*conn, getPath(node).c_str(), Host::default_service, node), node_(node), seq_mcu_comm_handler_(seq_mcu_comm_handler)
{
    // TODO
    // Register Event listener here:
    // seq_mcu_comm_handler_.listener_handlers.push_back(EVENT_FUNC);
    determineInitialState();

    // TODO(BMC-15): For Titanium and Tacton SMBus communication to the sequence MCU is isolated below S0
    // so we can only support Transition::Reboot
    allowedHostTransitions({Transition::On, Transition::Off, Transition::Reboot});
}

std::string Host::getPath(const std::string& node)
{
    return std::string(Host::namespace_path::value) + "/" + std::string(Host::namespace_path::host) + node;
}

Host::Transition Host::requestedHostTransition(Transition value)
{
    info("Host{NODE}: Requested host transition from {OLD} to {NEW}", 
        "NODE", node_, "OLD", 
            sdbusplus::server::xyz::openbmc_project::state::Host::requestedHostTransition(), 
            "NEW", value);

    switch (value) {
        case(Host::Transition::On) : {
            // smbus_.SmbusWriteByte(0x04, 0x00);
            seq_mcu_comm_handler_.IssueAwakeCmd();
            info("Awake Signal written");
            break;
        } case (Host::Transition::Off) : {
            // smbus_.SmbusWriteByte(0x04, 0x02);
            seq_mcu_comm_handler_.IssueSoftShutdown();
            info("Soft Shutdown Signal written");
            break;
        } case(Host::Transition::Reboot) :
            case(Host::Transition::GracefulWarmReboot) :
            info("Soft Reset Signal Written");
            seq_mcu_comm_handler_.IssueSoftReset();
            break;
        case(Host::Transition::ForceWarmReboot) : 
            info("Hard Reset Signal Written");
            seq_mcu_comm_handler_.IssueHardReset();
            break;
        default : {
            error("Unhandled Request");
            break;
        }
    }
    
    sdbusplus::server::xyz::openbmc_project::state::Host::requestedHostTransition(value);
    return value;
}

Host::Transition Host::requestedHostTransition() const
{
    return sdbusplus::server::xyz::openbmc_project::state::Host::requestedHostTransition();
}

Host::HostState Host::currentHostState(HostState value)
{
    info("Host{NODE}: Current host state change to {NEW}", "NODE", node_, "NEW", value);
    return sdbusplus::server::xyz::openbmc_project::state::Host::currentHostState(value);
}

Host::HostState Host::currentHostState() const
{
    McuPowerState raw_state;
    auto status = seq_mcu_comm_handler_.GetMcuPowerState(raw_state);
    
    if (status != SMBUSOperationStatus::kSMBUSOperationStatus_Success) {
        error("GET state failed: {STATUS}", "STATUS", std::to_underlying(status));
        return sdbusplus::server::xyz::openbmc_project::state::Host::currentHostState();
    } 
    
    info("GET raw state: {STATE}", "STATE", std::to_underlying(raw_state));

    return toHostState(raw_state);
}

Host::RestartCause Host::restartCause(RestartCause value)
{
    // Noop hook: add logic here if needed
    info("Host{NODE}: Restart cause change to {NEW}", "NODE", node_, "NEW", value);
    return sdbusplus::server::xyz::openbmc_project::state::Host::restartCause(value);
}

Host::RestartCause Host::restartCause() const
{
    return sdbusplus::server::xyz::openbmc_project::state::Host::restartCause();
}

void Host::determineInitialState()
{
    std::pair<McuPowerState, TransitionCause> raw_state_cause;
    auto status = seq_mcu_comm_handler_.GetStateAndTransitionCause(raw_state_cause);

    info("GetStateAndTransitionCause: {STATUS}, raw_state: {STATE}, raw_cause: {CAUSE}",
        "STATUS", std::to_underlying(status),
        "STATE", std::to_underlying(raw_state_cause.first),
        "CAUSE", std::to_underlying(raw_state_cause.second));

    // (BMC-19): currentHostState should be determined by querying the sequence MCU for the actual host state
    // (BMC-80): restartCause should be determined by querying the sequence MCU or BIOS for the actual restart cause
    if (status != SMBUSOperationStatus::kSMBUSOperationStatus_Success) {
        return; 
    }

    HostState mapped_state = toHostState(raw_state_cause.first);
    currentHostState(mapped_state);

    RestartCause mapped_cause = toRestartCause(raw_state_cause.second);
    restartCause(mapped_cause);


    // (BMC-19): requestedPowerTransition should match the currentPowerState on BMC startup
    Host::Transition type_to_init;  
    if (mapped_state == HostState::Running ||
        mapped_state == HostState::TransitioningToRunning) {
        type_to_init = Transition::On;
    } else {
        type_to_init = Transition::Off;
    }

    requestedHostTransition(type_to_init);
}