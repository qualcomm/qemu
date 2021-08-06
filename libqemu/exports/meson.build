export = meson.source_root() / 'libqemu/exports.py'
wrap = meson.source_root() / 'scripts/libqemu-wrappers.py'

headers_to_install = [ 'typedefs.h', 'struct.h' ]

foreach file_name : ['fill.c',
                     'iothread-wrapper.c',
                     'typedefs.h',
                     'struct.h',
                     'iothread-wrapper.h'
                     ]
    param = '--' + '-'.join(file_name.split('.'))

    file = custom_target(file_name,
                         output: file_name,
                         input: export,
                         command: [ python, wrap, param, export ],
                         install : headers_to_install.contains(file_name),
                         install_dir : join_paths(get_option('includedir'), 'libqemu/libqemu/exports'),
                         capture: true)
    libqemu_ss.add(file)

endforeach
