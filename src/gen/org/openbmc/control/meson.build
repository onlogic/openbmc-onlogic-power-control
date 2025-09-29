# Generated file; do not modify.
subdir('Power')

sdbusplus_current_path = 'org/openbmc/control'

generated_markdown += custom_target(
    'org/openbmc/control/Power__markdown'.underscorify(),
    input: [ '../../../../yaml/org/openbmc/control/Power.interface.yaml',  ],
    output: [ 'Power.md' ],
    depend_files: sdbusplusplus_depfiles,
    command: [
        sdbuspp_gen_meson_prog, '--command', 'markdown',
        '--output', meson.current_build_dir(),
        '--tool', sdbusplusplus_prog,
        '--directory', meson.current_source_dir() / '../../../../yaml',
        'org/openbmc/control/Power',
    ],
    install: should_generate_markdown,
    install_dir: [inst_markdown_dir / sdbusplus_current_path],
    build_by_default: should_generate_markdown,
)

