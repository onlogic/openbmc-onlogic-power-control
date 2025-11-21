#pragma once

#include "object_server.hpp"
#include "sequence_mcu_handler.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>

class Host : public ObjectServer<sdbusplus::server::xyz::openbmc_project::state::Host>
{
public:
    using Transition = sdbusplus::server::xyz::openbmc_project::state::Host::Transition;
    using HostState = sdbusplus::server::xyz::openbmc_project::state::Host::HostState;
    using RestartCause = sdbusplus::server::xyz::openbmc_project::state::Host::RestartCause;

    static const std::unordered_map<McuPowerState, Host::HostState> nativeToHostState;
    static const std::unordered_map<TransitionCause, Host::RestartCause> mapTransitionCauseToRestartCause;

    Host(std::shared_ptr<sdbusplus::asio::connection> conn, const std::string& node, SequenceMCUHandler& seq_mcu_comm_handler);

    // TODO
    // void MCUUpdateHandler() {
    //     seq_mcu_comm_handler_.GetPowerState()
    //     // do mapping
    //     // do setting of base class values
    // }

    /// @brief Sets the requested host transition
    /// @param value - Can be one of Off, On, Reboot, GracefulWarmReboot, ForceWarmReboot
    /// @return - The updated requested host transition
    Transition requestedHostTransition(Transition value) override;

    /// @brief Gets the requested host transition
    /// @return - The requested host transition: Off, On, Reboot, GracefulWarmReboot, ForceWarmReboot
    Transition requestedHostTransition() const override;

    /// @brief Sets the current host state
    /// @param value - The new current host state
    /// @return - The updated current host state
    HostState currentHostState(HostState value) override;

    HostState currentHostState() const override;

    RestartCause restartCause(RestartCause value) override;

    RestartCause restartCause() const override;

    static constexpr const char* default_service = "xyz.openbmc_project.State.Host";
    static std::string getPath(const std::string& node);

private:
    /// @brief Interogates the system to determine the initial host state on BMC startup
    void determineInitialState();

    const std::string node_;
    SequenceMCUHandler& seq_mcu_comm_handler_;
};
