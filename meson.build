project(
  'openbmc-onlogic-power-control',
  'cpp',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

dependencies = [
  dependency('libsystemd'),
]

exe = executable(
  'openbmc-onlogic-power-control',
  'src/openbmc_onlogic_power_control.cpp',
  include_directories: include_directories('src'),
  dependencies : dependencies,
  install : true,
  install_dir: get_option('bindir'),
)

systemd = dependency('systemd')
if systemd.found()
  install_data(
    'service_files/openbmc-onlogic-power-control.service',
    install_dir: systemd.get_variable('systemd_system_unit_dir'),
  )
endif