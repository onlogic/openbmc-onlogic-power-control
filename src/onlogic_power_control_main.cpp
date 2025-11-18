#include <phosphor-logging/lg2.hpp>

#include <sdbusplus/server.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <boost/asio/io_context.hpp>

// DBus interface includes
#include <org/openbmc/control/Power/server.hpp>         // Generated from src/yaml/org/openbmc/control/Power.interface.yaml
#include <xyz/openbmc_project/State/Chassis/server.hpp> // Generated from subprojects/phosphor-dbus-interfaces/yaml/xyz/openbmc_project/State/Chassis.interface.yaml
#include <xyz/openbmc_project/State/Host/server.hpp>    // Generated from subprojects/phosphor-dbus-interfaces/yaml/xyz/openbmc_project/State/Host.interface.yaml
#include <xyz/openbmc_project/Common/error.hpp>         // Generated from subprojects/phosphor-dbus-interfaces not sure the exact yaml file

#include "smbus_manager.hpp"
#include "sequence_mcu_handler.hpp"

PHOSPHOR_LOG2_USING;

static uint64_t getCurrentTimeMs() {
    struct timespec time = {};

    if (clock_gettime(CLOCK_REALTIME, &time) < 0) {
        return 0;
    }

    uint64_t currentTimeMs = static_cast<uint64_t>(time.tv_sec) * 1000;
    currentTimeMs += static_cast<uint64_t>(time.tv_nsec) / 1000 / 1000;

    return currentTimeMs;
}

template <typename T> class ObjectServer : public sdbusplus::server::object_t<T> {
public:
    ObjectServer(sdbusplus::bus_t& bus, const char* path, const char* interface, const std::string& node)
        : sdbusplus::server::object_t<T>(bus, path), objectManager(bus, path) 
        {
            bus.request_name((std::string(interface) + node).c_str());
            if (node == "0")
            {
                bus.request_name(interface);
            }
        }

protected:
    const sdbusplus::server::manager_t objectManager;
};

class Power : ObjectServer<sdbusplus::server::org::openbmc::control::Power>
{
public:
    Power(std::shared_ptr<sdbusplus::asio::connection> conn, const std::string& node)
        : ObjectServer(*conn, sdbusplus::server::org::openbmc::control::Power::instance_path, sdbusplus::server::org::openbmc::control::Power::default_service, node)
    {
        // Initially signal that power is good
        // We don't really care about this signal it just needs to be high for BMC::Ready
        pgood(1);
    };
};

class Chassis : ObjectServer<sdbusplus::server::xyz::openbmc_project::state::Chassis>
{
public:
    Chassis(std::shared_ptr<sdbusplus::asio::connection> conn, const std::string& node, SequenceMCUHandler& seq_mcu_comm_handler)
        : ObjectServer(*conn, getPath(node).c_str(), Chassis::default_service, node), node(node), seq_mcu_comm_handler_(seq_mcu_comm_handler)
    {
        determineInitialState();
    };

private:
    const std::string node;
    SequenceMCUHandler& seq_mcu_comm_handler_;

public:

    /// @brief Sets the requested power transition state of the chassis
    /// @param value - Can be one of Off, On, or PowerCycle
    /// @return 
    Transition requestedPowerTransition(Transition value)
    {
        // info("Chassis{NODE}: Requested power transition from {OLD} to {NEW}", "NODE", node, "OLD", sdbusplus::server::xyz::openbmc_project::state::Chassis::requestedPowerTransition(), "NEW", value);
        switch (value) {
            case(Chassis::Transition::On) : {
                // smbus_.SmbusWriteByte(0x04, 0x00);
                seq_mcu_comm_handler_.IssueAwakeCmd();
                info("Awake Signal written");
                break;
            } case (Chassis::Transition::Off) : {
                // smbus_.SmbusWriteByte(0x04, 0x03);
                seq_mcu_comm_handler_.IssueHardShutdown();
                info("Hard Off Signal written");
                break;
            } default : {
                error("Unhandled Request");
                break;
            }
        }

        return sdbusplus::server::xyz::openbmc_project::state::Chassis::requestedPowerTransition(value);
    }

    /// @brief Gets the requested power transition state of the chassis
    /// @return - The requested power transition state: Off, On, or PowerCycle
    Transition requestedPowerTransition() const
    {
        return sdbusplus::server::xyz::openbmc_project::state::Chassis::requestedPowerTransition();
    }

    /// @brief Sets the current power state of the chassis
    /// @param value - Can be one of Off, On, TransitioningToOff, TransitioningToOn
    /// @return - The updated power state
    PowerState currentPowerState(PowerState value)
    {
        info("Chassis{NODE}: Current power state change to {NEW}", "NODE", node, "NEW", value);
        sdbusplus::server::xyz::openbmc_project::state::Chassis::currentPowerState(value);
        lastStateChangeTime(getCurrentTimeMs());
        return value;
    }

    /// @brief Gets the current power state of the chassis
    /// @return - The current power state of the chassis: Off, On, TransitioningToOff, TransitioningToOn
    PowerState currentPowerState() const
    {
        return sdbusplus::server::xyz::openbmc_project::state::Chassis::currentPowerState();
    }

    /// @brief Sets the current power status of the chassis (Should only be Good, Brownout or Undefined based on what information we can get from sequence MCU)
    /// @param value - Can be one of Good, Brownout, UninterruptiblePowerSupply, Undefined
    /// @return - The updated power status
    PowerStatus currentPowerStatus(PowerStatus value)
    {
        // Noop hook: add logic here if needed        
        info("Chassis{NODE}: Current power status change to {NEW}", "NODE", node, "NEW", value);
        sdbusplus::server::xyz::openbmc_project::state::Chassis::currentPowerStatus(value);
        lastStateChangeTime(getCurrentTimeMs());
        return value;
    }

    /// @brief Gets the current power status of the chassis
    /// @return - The current power status of the chassis: Good, Brownout, UninterruptiblePowerSupply, Undefined
    PowerStatus currentPowerStatus() const
    {
        return sdbusplus::server::xyz::openbmc_project::state::Chassis::currentPowerStatus();
    }

private:
    /// @brief Interogates the system to determine the initial chassis state on BMC startup
    void determineInitialState()
    {
        // We will just always assume Power Good if the BMC has power for now. 
        // We can improve this later by including brownout detection and UPS status when available from the sequence MCU
        currentPowerStatus(PowerStatus::Good);

        // TODO(BMC-19): currentPowerState should be determined by querying the sequence MCU for the actual chassis power state
        currentPowerState(PowerState::On);

        // TODO(BMC-19): requestedPowerTransition should match the currentPowerState on BMC startup
        requestedPowerTransition(Transition::On);

        lastStateChangeTime(getCurrentTimeMs());
    }

public:
    static constexpr const char* default_service = "xyz.openbmc_project.State.Chassis";

    /// @brief Constructs the DBus object path for the chassis based on the node
    /// @param node - The node identifier, currently this should always be "0"
    /// @return - The DBus object path for the chassis
    static std::string getPath(const std::string& node)
    {
        return std::string(Chassis::namespace_path::value) + "/" + std::string(Chassis::namespace_path::chassis) + node;
    }
};

class Host : ObjectServer<sdbusplus::server::xyz::openbmc_project::state::Host>
{
public:
    Host(std::shared_ptr<sdbusplus::asio::connection> conn, const std::string& node, SequenceMCUHandler& seq_mcu_comm_handler)
        : ObjectServer(*conn, getPath(node).c_str(), Host::default_service, node), node_(node), seq_mcu_comm_handler_(seq_mcu_comm_handler)
    {
        determineInitialState();

        // TODO(BMC-15): For Titanium and Tacton SMBus communication to the sequence MCU is isolated below S0
        // so we can only support Transition::Reboot
        allowedHostTransitions({Transition::On, Transition::Off, Transition::Reboot});
    };

private:
    const std::string node_;
    SequenceMCUHandler& seq_mcu_comm_handler_;
public:
    /// @brief Sets the requested host transition
    /// @param value - Can be one of Off, On, Reboot, GracefulWarmReboot, ForceWarmReboot
    /// @return - The updated requested host transition
    Transition requestedHostTransition(Transition value)
    {
        // Noop hook: add logic here if needed
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

    /// @brief Gets the requested host transition
    /// @return - The requested host transition: Off, On, Reboot, GracefulWarmReboot, ForceWarmReboot
    Transition requestedHostTransition() const
    {
        return sdbusplus::server::xyz::openbmc_project::state::Host::requestedHostTransition();
    }

    /// @brief Sets the current host state
    /// @param value - The new current host state
    /// @return - The updated current host state
    HostState currentHostState(HostState value)
    {
        info("Host{NODE}: Current host state change to {NEW}", "NODE", node_, "NEW", value);
        sdbusplus::server::xyz::openbmc_project::state::Host::currentHostState(value);
        return value;
    }

    HostState currentHostState() const
    {
        sdbusplus::common::xyz::openbmc_project::state::Host::HostState host_state;
        auto status = seq_mcu_comm_handler_.GetPowerState(host_state);
        if (status != SMBUSOperationStatus::kSMBUSOperationStatus_Success) {
            error("GET state failed: {STATUS}", "STATUS", std::to_underlying(status));
            return HostState::Off; // there are no better placeholders
        } else {
            info("GET state: {STATE}", "STATE", std::to_underlying(host_state));
        }
        return host_state;
    }

    RestartCause restartCause(RestartCause value)
    {
        // Noop hook: add logic here if needed
        info("Host{NODE}: Restart cause change to {NEW}", "NODE", node_, "NEW", value);
        sdbusplus::server::xyz::openbmc_project::state::Host::restartCause(value);
        return value;
    }

    RestartCause restartCause() const
    {
        return sdbusplus::server::xyz::openbmc_project::state::Host::restartCause();
    }

private:
    /// @brief Interogates the system to determine the initial host state on BMC startup
    void determineInitialState()
    {
        // TODO(BMC-19): currentHostState should be determined by querying the sequence MCU for the actual host state
        currentHostState(HostState::Running);
        // TODO(BMC-19): requestedHostTransition should match the currentHostState on BMC startup
        requestedHostTransition(Transition::On);

        // TODO(BMC-80): restartCause should be determined by querying the sequence MCU or BIOS for the actual restart cause
        restartCause(RestartCause::Unknown);
    }

public:
    static constexpr const char* default_service = "xyz.openbmc_project.State.Host";

    static std::string getPath(const std::string& node)
    {
        return std::string(Host::namespace_path::value) + "/" + std::string(Host::namespace_path::host) + node;
    }
};

int run_i2c_tests() {
    SMBUSManager smbus("/dev/i2c-1", 0x40);
    smbus.InitSMBUSManager();

    uint8_t output;
    int operation_status;

    operation_status = smbus.SmbusSubaddressReadByte(0x00, &output);
    if (operation_status < 0) {
        return -1;
    }
    info("GET state: {STATE}", "STATE", output);
    sleep(1);

    operation_status = smbus.SmbusSubaddressReadByte(0x01, &output);
    if (operation_status < 0) {
        return -1;
    }
    info("GET_TRANSITION_CAUSE: {GET_TRANSITION_CAUSE}", "GET_TRANSITION_CAUSE", output);
    sleep(1);
    
    operation_status = smbus.SmbusSubaddressReadByte(0x03, &output);
    if (operation_status < 0) {
        return -1;
    }
    info("GET_CAPABILITIES: {GET_CAPABILITIES}", "GET_CAPABILITIES", output);
    sleep(1);

    uint8_t buf[2];
    size_t size_read;
    operation_status = smbus.SmbusSubaddressReadByteBlock(0x02, 2, buf, &size_read);
    if (operation_status == 0) {
    info("BLOCK READ (subaddress 0x02): size_read={SIZE}, buf[0]={B0_DEC}, buf[1]={B1_DEC}",
         "SIZE", size_read,
         "B0_DEC", buf[0],
         "B1_DEC", buf[1]);
    } else {
        error("BLOCK READ failed: {STATUS}", "STATUS", operation_status);
    }

    // ***********************************************************************

    const uint8_t OPERATION_TESTS = 3; 
    uint8_t operation_tests[OPERATION_TESTS] = { 0x00, 0x01, 0x03 };
    uint8_t write_operation;
    uint8_t subaddress = 0x04;

    int write_status = 0;
    for(int i = 0; i < OPERATION_TESTS; ++i) {
        write_operation = operation_tests[i];
        info("OPERATION: {WRITE_OPERATION}", "WRITE_OPERATION", write_operation);
        write_status = smbus.SmbusWriteByte(subaddress, write_operation);
        info("Write Status: {WRITE_STATUS}", "WRITE_STATUS", write_status);
        sleep(5);
    }

    return 0;
}

void run_handler_tests() {
    SMBUSManager smbus_manager("/dev/i2c-1", 0x40);
    smbus_manager.InitSMBUSManager();

    SequenceMCUHandler handler(smbus_manager);

    // --- GET COMMANDS FIRST ---
    sdbusplus::common::xyz::openbmc_project::state::Host::HostState host_state;
    auto status = handler.GetPowerState(host_state);
    info("GetPowerState: {STATUS}, state: {STATE}",
         "STATUS", std::to_underlying(status),
         "STATE", std::to_underlying(host_state));
    sleep(1);

    sdbusplus::common::xyz::openbmc_project::state::Host::RestartCause restart_cause;
    status = handler.GetTransitionCause(restart_cause);
    info("GetTransitionCause: {STATUS}, cause: {CAUSE}",
         "STATUS", std::to_underlying(status),
         "CAUSE", std::to_underlying(restart_cause));
    sleep(1);

    std::pair<
        sdbusplus::common::xyz::openbmc_project::state::Host::HostState,
        sdbusplus::common::xyz::openbmc_project::state::Host::RestartCause
    > state_and_cause;
    status = handler.GetStateAndTransitionCause(state_and_cause);
    info("GetStateAndTransitionCause: {STATUS}, state: {STATE}, cause: {CAUSE}",
         "STATUS", std::to_underlying(status),
         "STATE", std::to_underlying(state_and_cause.first),
         "CAUSE", std::to_underlying(state_and_cause.second));
    sleep(1);

    SMBUSCapability capability;
    status = handler.GetCapability(capability);
    info("GetCapability: {STATUS}, capability: {CAP}",
         "STATUS", std::to_underlying(status),
         "CAP", std::to_underlying(capability));
    sleep(1);

    // --- SET COMMANDS LAST ---
    status = handler.IssueAwakeCmd();
    info("IssueAwakeCmd: {STATUS}", "STATUS", std::to_underlying(status));
    sleep(5);

    // hold off on this for now
    // status = handler.IssueSoftReset();
    // info("IssueSoftReset: {STATUS}", "STATUS", std::to_underlying(status));
    // sleep(5);

    status = handler.IssueHardReset();
    info("IssueHardReset: {STATUS}", "STATUS", std::to_underlying(status));
    sleep(5);

    status = handler.IssueSoftShutdown();
    info("IssueSoftShutdown: {STATUS}", "STATUS", std::to_underlying(status));
    sleep(5);

    status = handler.IssueAwakeCmd();
    info("IssueAwakeCmd: {STATUS}", "STATUS", std::to_underlying(status));
    sleep(5);

    status = handler.IssueHardShutdown();
    info("IssueHardShutdown: {STATUS}", "STATUS", std::to_underlying(status));
    sleep(5);
}

int main(int argc, char* argv[]) {
    static std::string node = "0";
    if (argc > 1) {
        node = argv[1];
    }

    if (node != "0") {
        error("Host{NODE}: Non-0 host not supported...", "NODE", node);
        return -1;
    }

    SMBUSManager smbus_manager("/dev/i2c-1", 
                               std::to_underlying(SlaveAddressTable::kSlaveAddress_SequenceMCU));

    smbus_manager.InitSMBUSManager();

    SequenceMCUHandler seq_mcu_comm_handler(smbus_manager);

    boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);

    Power power0(conn, node);
    Chassis chassis0(conn, node, seq_mcu_comm_handler);
    Host host0(conn, node, seq_mcu_comm_handler);

    io.run();
}
