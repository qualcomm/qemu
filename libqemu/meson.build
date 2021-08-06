if not enable_libqemu
   subdir_done()
endif

libqemu_ss.add(files('ctors.c', 'run-on-iothread.c', 'libqemu.c'))

subdir('wrappers')
subdir('exports')

install_headers('../include/exec/hwaddr.h', '../include/exec/memattrs.h', subdir : 'libqemu/exec')
install_headers('libqemu.h', subdir : 'libqemu/libqemu')
