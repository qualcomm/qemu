export = meson.source_root() / 'libqemu/exports.py'
wrap = meson.source_root() / 'scripts/libqemu-wrappers.py'

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
                         capture: true)
    libqemu_ss.add(file)

endforeach
