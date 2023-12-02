hexagon_ss = ss.source_set()
hexagon_ss.add(when: 'CONFIG_HEX_DSP', if_true: files('hexagon_dsp.c', 'hexagon_globalreg.c'))

hw_arch += {'hexagon': hexagon_ss}

