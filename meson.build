project(
  'openbmc-onlogic-power-control',
  'cpp',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

dependencies = [
]

exe = executable(
  'openbmc-onlogic-power-control',
  'openbmc_onlogic_power_control.cpp',
  dependencies : dependencies,
  install : true,
  install_dir: get_option('bindir'),
)
