contrib_plugins = [
'bbv.c',
'cache.c',
'cflow.c',
'drcov.c',
'execlog.c',
'hotblocks.c',
'hotpages.c',
'howvec.c',
'hwprofile.c',
'ips.c',
'stoptrigger.c',
'traps.c',
'uftrace.c',
]

# libqemu-specific plugins that need libqemu headers
libqemu_plugins = [
'idlinker.c',
]

if host_os != 'windows'
  # lockstep uses socket.h
  contrib_plugins += 'lockstep.c'
endif

if 'cpp' in all_languages
  contrib_plugins += 'cpp.cpp'
endif

t = []
if get_option('plugins')
  foreach i : contrib_plugins
    if host_os == 'windows'
      t += shared_module(fs.stem(i), files(i) + 'win32_linker.c',
                        include_directories: ['../../include/qemu', '../../libqemu'],
                        link_depends: [win32_qemu_plugin_api_lib],
                        link_args: win32_qemu_plugin_api_link_flags,
                        dependencies: glib)
    else
      t += shared_module(fs.stem(i), files(i),
                        dependencies: plugins_deps,
                        install: true,
                        install_dir: get_option('libdir'))
    endif
  endforeach

  # Build libqemu-specific plugins with libqemu include directory
  foreach i : libqemu_plugins
    if host_os == 'windows'
      t += shared_module(fs.stem(i), files(i) + 'win32_linker.c',
                        include_directories: ['../../include/qemu', '../../libqemu'],
                        link_depends: [win32_qemu_plugin_api_lib],
                        link_args: win32_qemu_plugin_api_link_flags,
                        dependencies: glib)
    else
      t += shared_module(fs.stem(i), files(i),
                        include_directories: ['../../libqemu'],
                        dependencies: plugins_deps,
                        install: true,
                        install_dir: get_option('libdir'))
    endif
  endforeach
endif
if t.length() > 0
  alias_target('contrib-plugins', t)
else
  run_target('contrib-plugins', command: [python, '-c', ''])
endif

plugin_modules += t
