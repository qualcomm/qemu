if not enable_libqemu
   subdir_done()
endif

libqemu_ss.add(files('ctors.c', 'run-on-iothread.c'))

subdir('wrappers')
subdir('exports')
