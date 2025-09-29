#include <org/openbmc/control/Power/client.hpp>
#include <org/openbmc/control/Power/server.hpp>
#include <sdbusplus/server.hpp>

using PowerInherit = sdbusplus::server::object_t<sdbusplus::server::org::openbmc::control::Power>;

struct Power : PowerInherit
{
    Power(sdbusplus::bus_t& bus, const char* path) : PowerInherit(bus, path) {
        // TODO(BMC-32): Have BMC receive pgood signal from sequence MCU
        pgood(1);
    };
};

int main() {
    auto b = sdbusplus::bus::new_default();
    sdbusplus::server::manager_t m{b, Power::instance_path};

    b.request_name(Power::default_service);

    Power p0{b, Power::instance_path};

    b.process_loop();
}
