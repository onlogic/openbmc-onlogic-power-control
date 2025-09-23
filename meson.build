project(
  'openbmc-onlogic-power-control',
  'cpp',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

systemd = dependency('systemd')

bindir = get_option('bindir')
systemd_system_unit_dir = systemd.get_variable('systemd_system_unit_dir')

libsystemd = dependency('libsystemd')

subdir('src')
subdir('service_files')