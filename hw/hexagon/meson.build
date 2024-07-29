hexagon_ss = ss.source_set()
hexagon_ss.add(files('hexagon_tlb.c'))
hexagon_ss.add(when: 'CONFIG_HEX_DSP', if_true: files('hexagon_dsp.c', 'hexagon_globalreg.c'))

hw_arch += {'hexagon': hexagon_ss}

hexagon_ss.add(when: 'CONFIG_HEX_VIRT', if_true: files('virt.c', 'hexagon_globalreg.c'))

