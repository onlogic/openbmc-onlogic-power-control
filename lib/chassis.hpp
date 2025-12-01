#pragma once

#include "object_server.hpp"
#include "sequence_mcu_handler.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>

class Chassis : public ObjectServer<sdbusplus::server::xyz::openbmc_project::state::Chassis>
{
public:
    using Transition = sdbusplus::server::xyz::openbmc_project::state::Chassis::Transition;
    using PowerState = sdbusplus::server::xyz::openbmc_project::state::Chassis::PowerState;
    using PowerStatus = sdbusplus::server::xyz::openbmc_project::state::Chassis::PowerStatus;
;
    static const std::unordered_map<Chassis::Transition, uint8_t> chassisTransitionToNative;
    static const std::unordered_map<uint8_t, Chassis::PowerState> nativeToChassisState;
    static const std::unordered_map<Chassis::PowerState, uint8_t> chassisStateToNative;
    static const std::unordered_map<uint8_t, Chassis::PowerStatus> mapTransitionCauseToChassisPowerStatus;
    
    Chassis(std::shared_ptr<sdbusplus::asio::connection> conn, const std::string& node, SequenceMCUHandler& seq_mcu_comm_handler);

    /// @brief Sets the requested power transition state of the chassis
    /// @param value - Can be one of Off, On, or PowerCycle
    /// @return 
    Transition requestedPowerTransition(Transition value) override;

    /// @brief Gets the requested power transition state of the chassis
    /// @return - The requested power transition state: Off, On, or PowerCycle
    Transition requestedPowerTransition() const override;

    /// @brief Sets the current power state of the chassis
    /// @param value - Can be one of Off, On, TransitioningToOff, TransitioningToOn
    /// @return - The updated power state
    PowerState currentPowerState(PowerState value) override;

    /// @brief Gets the current power state of the chassis
    /// @return - The current power state of the chassis: Off, On, TransitioningToOff, TransitioningToOn
    PowerState currentPowerState() const override;

    /// @brief Sets the current power status of the chassis (Should only be Good, Brownout or Undefined based on what information we can get from sequence MCU)
    /// @param value - Can be one of Good, Brownout, UninterruptiblePowerSupply, Undefined
    /// @return - The updated power status
    PowerStatus currentPowerStatus(PowerStatus value) override;

    /// @brief Gets the current power status of the chassis
    /// @return - The current power status of the chassis: Good, Brownout, UninterruptiblePowerSupply, Undefined
    PowerStatus currentPowerStatus() const override;

    static constexpr const char* default_service = "xyz.openbmc_project.State.Chassis";

    /// @brief Constructs the DBus object path for the chassis based on the node
    /// @param node - The node identifier, currently this should always be "0"
    /// @return - The DBus object path for the chassis
    static std::string getPath(const std::string& node);

private:
    /// @brief Interogates the system to determine the initial chassis state on BMC startup
    void determineInitialState();

    const std::string node;
    SequenceMCUHandler& seq_mcu_comm_handler_;
};
