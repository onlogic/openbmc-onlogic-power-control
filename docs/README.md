---
description: Power control implementation for OpenBMC
---

# OpenBMC Power Control

For details on the DBus interfaces that are implemented in this package, or information about how to add new interfaces, see [dbus-interfaces.md](dbus-interfaces.md "mention").

## Rapid Development and Debugging

To perform a tighter development loop (including onboard debugging) you will need to have the following pre-requisites in place:

1. Image needs to be configured with GDB installed (See [the development guide for details](https://github.com/OnLogic-Engineering/openbmc-onlogic/blob/master/docs/development.md))

    Add the following to your `meta-onlogic/conf/layer.conf`:
    ```
    IMAGE_INSTALL:append = " onlogic-power-control-dbg " # Replacing onlogic-power-control with the debug version
    EXTRA_IMAGE_FEATURES:append = " tools-debug" # Add GDB to the image
    ```
1. OnLogic Power Control recipe needs to be pulling from your remote development branch

    Edit the following line in `meta-onlogic/recipes-onlogic/onlogic-power-control/onlogic-power-control_git.bb` replacing ${YOUR_DEV_BRANCH} with the name of your dev branch:
    ```
    SRC_URI = "git://git@github.com/OnLogic-Engineering/openbmc-onlogic-power-control.git;branch=${YOUR_DEV_BRANCH};protocol=ssh"
    ```

Steps to copy over a new build and debug:

1. Commit your changes to `openbmc-onlogic-power-control` and push them to your remote development branch 
1. Open your `openbmc-onlogic` docker build environment for the your desired board:
    ```
    docker run ... openbmc_build bash
    . setup <board_name> /build/<board_name>
    ```
1. Specifically build `onlogic-power-control`
    ```
    bitbake onlogic-power-control
    ```
1. Enter a devshell for the `onlogic-power-control` recipe:
    ```
    bitbake -c devshell onlogic-power-control
    ```
1. In another terminal make sure the `onlogic-power-control\@0.service` is stopped on your development board. Copying the files over will fail if they're in use.
    ```
    ssh root@${BOARD_IP}
    systemctl stop onlogic-power-control\@0
    ```
1. Copy the build and debug assets over to your dev board, replacing ${BOARD_IP} with your development board's ip:
    ```
    cd ..
    scp packages-split/onlogic-power-control-dbg/usr/bin/.debug/onlogic-power-control root@${BOARD_IP}:/usr/bin/.debug/onlogic-power-control
    scp packages-split/onlogic-power-control/usr/bin/onlogic-power-control root@${BOARD_IP}:/usr/bin/onlogic-power-control
    scp package/usr/src/debug/onlogic-power-control/0.1+gitAUTOINC/src/onlogic_power_control_main.cpp root@${BOARD_IP}:/usr/src/debug/onlogic-power-control/0.1+gitAUTOINC/src/
    ```
    You'll need to do each SCP command independently as you'll need to type the root user password for your board.
1. Debug an instance of the application (See [the GDB Cheat Sheet](https://darkdust.net/files/GDB%20Cheat%20Sheet.pdf) or Google if you need help debugging)
    ```
    ssh root@${BOARD_IP}
    gdb /usr/bin/onlogic-power-control
    ```

