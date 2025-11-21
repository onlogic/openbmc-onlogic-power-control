#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <boost/asio/io_context.hpp>
#include <org/openbmc/control/Power/server.hpp>

#include "smbus_manager.hpp"
#include "sequence_mcu_handler.hpp"
#include "object_server.hpp"
#include "chassis.hpp"
#include "host.hpp"

PHOSPHOR_LOG2_USING;

class Power : public ObjectServer<sdbusplus::server::org::openbmc::control::Power>
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

    SMBUSManager smbus_manager("/dev/i2c-1", 
        std::to_underlying(SlaveAddressTable::kSlaveAddress_SequenceMCU));

    smbus_manager.InitSMBUSManager();

    SequenceMCUHandler seq_mcu_comm_handler(smbus_manager, io);

    auto conn = std::make_shared<sdbusplus::asio::connection>(io);

    Power power0(conn, node);
    Host host0(conn, node, seq_mcu_comm_handler);
    Chassis chassis0(conn, node, seq_mcu_comm_handler);

    seq_mcu_comm_handler.StartPolling();

    io.run();
}