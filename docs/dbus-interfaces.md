---
description: >-
  This page details how code gets generated for various DBus interfaces and the
  DBus interfaces that this project currently implements.
---

# Testing Interfaces

To test the Host and Chassis DBus interfaces you can either use the WebUI or the `obmcutil` cli tool.

## OBMC Util Tool

* `obmcutil poweroff` - Sets `requestedHostTransition` to `xyz.openbmc_project.State.Host.Transition.Off`

   This signal is used to send the host to S5 through graceful shutdown.
* `obmcutil poweron` - Sets `requestedHostTransition` to `xyz.openbmc_project.State.Host.Transition.On`

   This signal is used to wake the host into a S0 state, no matter the current Host or Chassis state.
* `obmcutil chassisoff` - Sets `requestedPowerTransition` to `xyz.openbmc_project.State.Chassis.Transition.Off`

   This signal is used to hard power off the system, it's a hard pull of the power.
* `obmcutil chassison` - Sets `requestedPowerTransition` to `xyz.openbmc_project.State.Chassis.Transition.On`

   This signal is used to turn power on to the PCH but not trigger a transition to Host On

*Note: Currently unaware of a mechanism to trigger the following Transitions from obmcutil:
   * Host: `xyz.openbmc_project.State.Host.Transition.GracefulWarmReboot`
   * Host: `xyz.openbmc_project.State.Host.Transition.ForceWarmReboot`

## WebUI Interfaces

Server Power Control is managed on the `Server Power Operations` page in the web ui. To get to this page:

1. Navigate to WebUI (https://<bmc_ip>) - Accepting the certificate warnings
1. Login - Username: `root` Password: `0penBmc`
1. Navigate to Server Power Operations page `Operations > Server power operations`

From here you can see the current server status:

1. Server Status
1. Last Power Operation

You can also initiate the following operations when the Server status is `On`:

1. Graceful Restart - Sets `requestedHostTransition` to `xyz.openbmc_project.State.Host.Transition.GracefulWarmReboot`

   This transition is meant to request a restart from the Host OS, enabling it to restart itself without pulling power from the PCH.
1. Force Restart - Sets `requestedHostTransition` to `xyz.openbmc_project.State.Host.Transition.ForceWarmReboot`

   This transition is meant to hard reset the OS by forcing the system to S5 and then back to S0.
1. Graceful Shutdown - Sets `requestedHostTransition` to `xyz.openbmc_project.State.Host.Transition.Off`

   This transition is meant to send the soft powerbutton signal to the Host OS requesting poweroff (or whatever the customer has powerbutton soft press set to).
1. Force Off - Sets `requestedPowerTransition` to `xyz.openbmc_project.State.Chassis.Transition.Off`

   This transition is meant to immediately pull power from the PCH (S5).

For debugging only, you can currently for set the Host State to `Off` using the following command `busctl set-property xyz.openbmc_project.State.Host0 /xyz/openbmc_project/state/host0 xyz.openbmc_project.State.Host CurrentHostState s "xyz.openbmc_project.State.Host.HostState.Off"` then the UI should show "Server status" == `Off` and under Operations you should have a `Power on` button. When you click this button it will set `requestedPowerTransition` to `xyz.openbmc_project.State.Host.Transition.On`.

# Current Interfaces

Currently the OpenBMC Power Control component implements the following interfaces.

1. org.openbmc.control.Power

   Definition: `./src/yaml/org/openbmc/control/Power.interface.yaml`\
   Generates: `<meson_build_dir>/src/gen/org/openbmc/control/Power/*`\
   This interface provides the `pgood` signal that OpenBMC depends upon to enter the BMC::Ready state. It signifies when the host chassis's power rails have stabilized.
1. xyz.openbmc_project.state.Host

   Definition: `./subprojects/phosphor-dbus-interfaces/yaml/xyz/openbmc_project/State/Host.interface.yaml`\
   Generates: `<meson_build_dir>/subprojects/phosphor-dbus-interfaces/gen/xyz/openbmc_project/State/Host/*`\
   This interface provides the `RequestedHostTransition`, `AllowedHostTransitions`, `CurrentHostState` and `RestartCause` properties related to Host system operation.
1. xyz.openbmc_project.state.Chassis

   Definition: `./subprojects/phosphor-dbus-interfaces/yaml/xyz/openbmc_project/State/Chassis.interface.yaml`\
   Generates: `<meson_build_dir>/subprojects/phosphor-dbus-interfaces/gen/xyz/openbmc_project/State/Chassis/*`\
   This interface provides the `RequestedPowerTransition`, `CurrentPowerState`, `CurrentPowerStatus` and `LastStateChangeTime` properties related to Host's Chassis power states.

## Custom DBus Interfaces

To generate DBus interface c++ code we use the [SDPlusPlus](https://github.com/openbmc/sdbusplus) project and its provided tooling. The steps for generating the c++ bindings are as follows:

1. Create or modify the yaml declaration files for the DBus interface you wish to implement\
   These files should be nested under the `src/yaml` directory based on their dbus path. So for example `org.openbmc.control.Power` goes in the file `src/yaml/org/openbmc/control/Power.{interface,events,errors}.yaml`
2. \[Optional: if you added new files] Regenerate the meson files to build the source\
   `./sdbusplus/tools/sdbus++-gen-meson --directory ./src/yaml --output ./src/gen --tool ./sdbusplus/tools/sdbus++`

This will leave you with a directory structure under `src/gen` matching the directory structure of `./src/yaml` library code will now get generated for those interfaces. Currently the only way to build is via the bitbake command in the OpenBMC project.&#x20;
