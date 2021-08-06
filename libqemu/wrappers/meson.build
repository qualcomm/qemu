libqemu_ss.add(files('memory.c', 'cpu.c', 'iothread.c', 'gpio.c', 'timer.c', 'chardev.c'))

subdir('target')

install_headers('gpio.h', 'timer.h', 'memory.h', 'cpu.h', 'libqemu.h',
                subdir : 'libqemu/libqemu/wrappers')
