#include <org/openbmc/control/Power/server.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <boost/asio/io_context.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>

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
    Power(sdbusplus::bus_t& bus, const std::string& node)
        : ObjectServer(bus, sdbusplus::server::org::openbmc::control::Power::instance_path, sdbusplus::server::org::openbmc::control::Power::default_service, node)
    {
        // TODO(BMC-32): Have BMC receive pgood signal from sequence MCU
        pgood(1);
    };
};

class Chassis : ObjectServer<sdbusplus::server::xyz::openbmc_project::state::Chassis>
{
public:
    Chassis(sdbusplus::bus_t& bus, const std::string& node)
        : ObjectServer(bus, getPath(node).c_str(), Chassis::default_service, node)
    {
        requestedPowerTransition(Transition::On);
        currentPowerState(PowerState::On);
        currentPowerStatus(PowerStatus::Good);
        lastStateChangeTime(getCurrentTimeMs());
    };

    static constexpr const char* default_service = "xyz.openbmc_project.State.Chassis";

    static std::string getPath(const std::string& node)
    {
        return std::string(Chassis::namespace_path::value) + "/" + std::string(Chassis::namespace_path::chassis) + node;
    }
};

class Host : ObjectServer<sdbusplus::server::xyz::openbmc_project::state::Host>
{
public:
    Host(sdbusplus::bus_t& bus, const std::string& node)
        : ObjectServer(bus, getPath(node).c_str(), Host::default_service, node)
    {
        requestedHostTransition(Transition::On);
        allowedHostTransitions({Transition::On, Transition::Off, Transition::Reboot});
        currentHostState(HostState::Running);
        restartCause(RestartCause::Unknown);
    };

    static constexpr const char* default_service = "xyz.openbmc_project.State.Host";

    static std::string getPath(const std::string& node)
    {
        return std::string(Host::namespace_path::value) + "/" + std::string(Host::namespace_path::host) + node;
    }
};

int main(int argc, char* argv[]) {
    static std::string node = "0";
    if (argc > 1) {
        node = argv[1];
    }

    if (node != "0") {
        error("Host{NODE}: Non-0 host not supported...", "NODE", node);
        return -1;
    }

    boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);

    Power power0(*conn, node);
    Chassis chassis0(*conn, node);
    Host host0(*conn, node);

    io.run();
}
