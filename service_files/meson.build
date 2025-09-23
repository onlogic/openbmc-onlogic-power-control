unit_files = [
    'openbmc-onlogic-power-control.service',
]

fs = import('fs')
foreach u : unit_files
    fs.copyfile(u, install: true, install_dir: systemd.get_variable('systemd_system_unit_dir'))
endforeach