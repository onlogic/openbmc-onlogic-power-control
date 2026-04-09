// SPDX-FileCopyrightText: (c) 2026 OnLogic, Inc. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
// SPDX-FileContributor: Justin Seely <justin.seely@onlogic.com>
// SPDX-FielContributor: Nicholas Hanna <nick.hanna@onlogic.com>  (SMBUS command set, MCU endpoint layer, MCU handler API, power control integration into DBus API, project structure)
#pragma once

#include <sdbusplus/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <string>

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
