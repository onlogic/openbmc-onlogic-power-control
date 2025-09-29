---
description: >-
  This page details how code gets generated for various DBus interfaces and the
  DBus interfaces that this project currently implements.
---

# DBus Interfaces

To generate DBus interface c++ code we use the [SDPlusPlus](https://github.com/openbmc/sdbusplus) project and its provided tooling. The steps for generating the c++ bindings are as follows:

1. Create or modify the yaml declaration files for the DBus interface you wish to implement\
   These files should be nested under the `src/yaml` directory based on their dbus path. So for example `org.openbmc.control.Power` goes in the file `src/yaml/org/openbmc/control/Power.{interface,events,errors}.yaml`
2. \[Optional: if you added new files] Regenerate the meson files to build the source\
   `./sdbusplus/tools/sdbus++-gen-meson --directory ./src/yaml --output ./src/gen --tool ./sdbusplus/tools/sdbus++`

This will leave you with a directory structure under `src/gen` matching the directory structure of `./src/yaml` library code will now get generated for those interfaces. Currently the only way to build is via the bitbake command in the OpenBMC project.&#x20;

## Current Interfaces

Currently the OpenBMC Power Control component implements the following interfaces.

1. org.openbmc.control.Power\
   This interface provides the `pgood` signal that OpenBMC depends upon to enter the BMC::Ready state. It signifies when the host chassis's power rails have stabilized.
