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
    t += shared_module(fs.stem(i), files(i),
                       include_directories: ['../../libqemu'],
                       dependencies: plugins_deps,
                       install: host_os != 'windows',
                       install_dir: get_option('libdir'))
  endforeach

  # Build libqemu-specific plugins with libqemu include directory
  foreach i : libqemu_plugins
    t += shared_module(fs.stem(i), files(i),
                       include_directories: ['../../libqemu'],
                       dependencies: plugins_deps,
                       install: true,
                       install_dir: get_option('libdir'))
  endforeach
endif
if t.length() > 0
  alias_target('contrib-plugins', t)
else
  run_target('contrib-plugins', command: [python, '-c', ''])
endif

plugin_modules += t
