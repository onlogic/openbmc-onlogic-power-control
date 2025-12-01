#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/program_options.hpp>
#include <org/openbmc/control/Power/server.hpp>

#include "smbus_manager.hpp"
#include "sequence_mcu_handler.hpp"
#include "object_server.hpp"
#include "chassis.hpp"
#include "host.hpp"

#include <iostream>

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
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("node,n", po::value<std::string>()->default_value("0"), "Node number of the host to manage")
        ("i2c-interface,i", po::value<std::string>()->default_value("/dev/i2c-1"), "I2C interface to use for SMBus communication");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

     if (vm.count("help")) {
        std::cerr << desc << "\n";
        return 1;
    }

    static std::string node = vm["node"].as<std::string>();
    static std::string i2cInterface = vm["i2c-interface"].as<std::string>();

    if (node != "0") {
        error("Host{NODE}: Non-0 host not supported...", "NODE", node);
        return -1;
    }

    boost::asio::io_context io;

    SMBUSManager smbus_manager(i2cInterface, 
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