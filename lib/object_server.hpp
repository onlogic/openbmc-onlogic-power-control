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
