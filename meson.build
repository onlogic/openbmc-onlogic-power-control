project(
  'openbmc-onlogic-power-control',
  'cpp',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

bindir = get_option('bindir')

systemd = dependency('systemd')

libsystemd = dependency('libsystemd')

subdir('src')
