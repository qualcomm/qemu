libqemu_ss.add(when: 'TARGET_AARCH64', if_true: files('arm.c'))
libqemu_ss.add(when: 'TARGET_RISCV32', if_true: files('riscv.c'))
libqemu_ss.add(when: 'TARGET_RISCV64', if_true: files('riscv.c'))

install_headers('riscv.h', subdir : 'libqemu/libqemu/wrappers/target')
