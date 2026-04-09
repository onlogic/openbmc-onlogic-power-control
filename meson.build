# SPDX-FileCopyrightText: (c) 2026 OnLogic, Inc. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileContributor: Justin Seely <justin.seely@onlogic.com>
# SPDX-FielContributor: Nicholas Hanna <nick.hanna@onlogic.com>  (SMBUS command set, MCU endpoint layer, MCU handler API, power control integration into DBus API, project structure)
project(
  'openbmc-onlogic-power-control',
  'cpp',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

bindir = get_option('bindir')

libsystemd = dependency('libsystemd')
sdbusplus = dependency('sdbusplus')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')
CLI11_dep = dependency('CLI11')

subdir('src')
