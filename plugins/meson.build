if not get_option('plugins')
  subdir_done()
endif

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

  if host_os == 'darwin'
    configure_file(
      input: qemu_plugins_symbol_list,
      output: 'qemu-plugins-ld64.symbols',
      capture: true,
      command: ['sed', '-ne', 's/^[[:space:]]*\\(\\(lib\\)\{0,1\}qemu_.*\\);/_\\1/p', '@INPUT@'])
    emulator_link_args += ['-Wl,-exported_symbols_list,plugins/qemu-plugins-ld64.symbols']
  else
    emulator_link_args += ['-Xlinker', '--dynamic-list=plugins/qemu-plugins.symbols']
  endif
endif

if host_os == 'windows'
  dlltool = find_program('dlltool', required: true)

  # Generate a .lib file for plugins to link against.
  # First, create a .def file listing all the symbols a plugin should expect to have
  # available in qemu
  win32_plugin_def = configure_file(
    input: files('qemu-plugins.symbols'),
    output: 'qemu_plugin_api.def',
    capture: true,
    command: ['sed', '-e', '0,/^/s//EXPORTS/; s/[{};]//g', '@INPUT@'])
  # then use dlltool to assemble a delaylib.
  win32_qemu_plugin_api_lib = configure_file(
    input: win32_plugin_def,
    output: 'libqemu_plugin_api.a',
    command: [dlltool, '--input-def', '@INPUT@',
              '--output-delaylib', '@OUTPUT@', '--dllname', 'qemu.exe']
  )
endif
specific_ss.add(files(
  'loader.c',
  'core.c',
  'api.c',
))
