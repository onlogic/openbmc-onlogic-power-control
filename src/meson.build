sdbusplusplus_prog = find_program('sdbus++', native: true)
sdbuspp_gen_meson_prog = find_program('sdbus++-gen-meson', native: true)
sdbusplusplus_depfiles = files()
if sdbusplus.type_name() == 'internal'
  sdbusplusplus_depfiles = subproject('sdbusplus').get_variable('sdbusplusplus_depfiles')
endif

yaml_selected_subdirs = ['org']
should_generate_cpp = true
should_generate_markdown = false
should_generate_registry = false
subdir('gen')

executable(
  'onlogic-power-control',
  'onlogic_power_control_main.cpp',
  'sequence_mcu_handler.cpp',
  'smbus_manager.cpp',
  'chassis.cpp',
  'host.cpp',
  generated_sources,
  dependencies : [
    sdbusplus,
    phosphor_dbus_interfaces,
    phosphor_logging,
  ],
  install : true,
  implicit_include_directories: false,
  include_directories: [
    include_directories('gen'),
    include_directories('../lib'),
  ],
  install_dir: bindir,
)
