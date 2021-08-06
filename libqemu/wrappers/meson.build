libqemu_ss.add(files('memory.c', 'cpu.c', 'iothread.c', 'gpio.c', 'timer.c'))

subdir('target')

install_headers('gpio.h', 'timer.h', 'memory.h',
                subdir : 'libqemu/libqemu/wrappers')
