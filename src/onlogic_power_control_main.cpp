// SPDX-FileCopyrightText: (c) 2026 OnLogic, Inc. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
// SPDX-FileContributor: Justin Seely <justin.seely@onlogic.com>
// SPDX-FielContributor: Nicholas Hanna <nick.hanna@onlogic.com>  (SMBUS command set, MCU endpoint layer, MCU handler API, power control integration into DBus API, project structure)
#include <CLI/CLI.hpp>
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
    CLI::App app{"OnLogic Power Control Service"};

    std::string node;
    app.add_option("-n,--node", node, "Node number of the host to manage")->default_val("0");
    std::string i2cInterface;
    app.add_option("-i,--i2c-interface", i2cInterface, "I2C interface to use for SMBus communication")->default_val("/dev/i2c-1");

    CLI11_PARSE(app, argc, argv);

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