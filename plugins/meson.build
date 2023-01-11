plugin_ldflags = []
# Modules need more symbols than just those in plugins/qemu-plugins.symbols
if not enable_modules
  conf_data = configuration_data()
  fs = import('fs')
  conf_data.set('qemu_plugins_symbols', fs.read('qemu-plugins.symbols'))
  if enable_libqemu
    conf_data.set('libqemu_symbols', fs.read('libqemu.symbols'))
  endif

  qemu_plugins_symbol_list = configure_file(
      input: files('qemu-plugins.symbols.in'),
      output: 'qemu-plugins.symbols',
      configuration: conf_data)

  if targetos == 'darwin'
    configure_file(
      input: qemu_plugins_symbol_list,
      output: 'qemu-plugins-ld64.symbols',
      capture: true,
      command: ['sed', '-ne', 's/^[[:space:]]*\\(\\(lib\\)\{0,1\}qemu_.*\\);/_\\1/p', '@INPUT@'])
    plugin_ldflags = ['-Wl,-exported_symbols_list,plugins/qemu-plugins-ld64.symbols']
  else
    plugin_ldflags = ['-Xlinker', '--dynamic-list=plugins/qemu-plugins.symbols']
  endif
endif

specific_ss.add(when: 'CONFIG_PLUGIN', if_true: [files(
  'loader.c',
  'core.c',
  'api.c',
), declare_dependency(link_args: plugin_ldflags)])
