hexagon_ss = ss.source_set()
hexagon_ss.add(files('hexagon_globalreg.c'))
hexagon_ss.add(when: 'CONFIG_DRAGON', if_true: files('hexagon_testboard.c',))
hexagon_ss.add(when: 'CONFIG_HEX_VIRT', if_true: files('virt.c',))


hw_arch += {'hexagon': hexagon_ss}

