#!/usr/bin/env python3

import sys
import re

indent_step = '    '
indent = 0

def gen_indent():
    for i in range(indent):
        print(indent_step, end = '')

def gen_c(s):
    if s != '':
        gen_indent()

    print(s)

def indent_push():
    global indent
    indent = indent + 1

def indent_pop():
    global indent
    indent = indent - 1

class PrivateInclude:
    includes = []

    def __init__(self, path, arch = None):
        self.path = path
        self.arch = arch
        self.includes.append(self)

    def gen():
        for i in PrivateInclude.includes:
            if i.arch:
                gen_c('#ifdef TARGET_{}'.format(i.arch.upper()))
            gen_c('#include "{}"'.format(i.path))
            if i.arch:
                gen_c('#endif')


class PublicInclude:
    includes = []

    def __init__(self, path):
        self.path = path
        self.includes.append(self)

    def gen():
        for i in PublicInclude.includes:
            gen_c('#include "{}"'.format(i.path))

class ExportedType:
    types = {}

    def __init__(self, pub, priv = None):
        priv = pub if priv is None else priv

        self.priv = priv
        self.pub = pub

        self.types[priv] = self

    def gen_pub_typedef_decl(self):
        gen_c('typedef struct {pub} {pub};'.format(pub = self.pub))

    def gen_pub_typedef_decls():
        for t in ExportedType.types.values():
            t.gen_pub_typedef_decl()

    def get_raw_type(t):
        raw = re.sub(r'\bconst\b', '', t)
        raw = re.sub(r'\*', '', raw).strip()

        return raw

    def priv_to_pub(priv_type):
        raw = ExportedType.get_raw_type(priv_type)

        if raw in ExportedType.types:
            pub = ExportedType.types[raw].pub
            ret = re.sub(r'\b' + raw + r'\b', pub, priv_type)
            return ret

        return priv_type

class ExportedFct:
    fcts = {}

    def __init__(self, pub, ret, args, priv = None, on_iothread = False, iothread_locked = False, arch = None):
        priv = pub if priv is None else priv

        self.pub = pub
        self.ret = ret
        self.args = args
        self.priv = priv
        self.on_iothread = on_iothread
        self.iothread_locked = iothread_locked
        self.wrapped = on_iothread or iothread_locked
        self.arch = arch

        self.fcts[priv] = self

    def get_pub_typedef_name(self):
        return self.pub + '_fn'

    def get_priv_target(self):
        return self.priv if not self.wrapped else self.priv + '_wrapper'

    def get_pub_cast(self):
        return '{} (*)({})'.format(
                ExportedType.priv_to_pub(self.ret),
                self.format_args(fmt = '{pub_type}'))

    def format_args(self, fmt = '{priv_type} p{idx}', sep = ', '):
        if len(self.args) == 0:
            if re.match(r'(\{priv_type\}|\{pub_type\})', fmt):
                return 'void'

        args = self.args

        args = [ fmt.format(
            priv_type = args[idx],
            pub_type = ExportedType.priv_to_pub(args[idx]),
            idx = idx) for idx in range(len(args)) ]
        return sep.join(args)

    def gen_pub_typedef_decl(self):
        gen_c('typedef {ret} (*{name})({args});'.format(
            ret = ExportedType.priv_to_pub(self.ret),
            name = self.get_pub_typedef_name(),
            args = self.format_args(fmt = '{pub_type}'))
        )

    def gen_pub_typedef_decls():
        for f in ExportedFct.fcts.values():
            f.gen_pub_typedef_decl()

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)

def gen_banner():
    gen_c('/* auto-generated - do not modify */')
    gen_c('')

def start_h_file(name):
    include_guard = '_LIBQEMU_' + name.upper().replace('-', '_') + '_H'

    gen_c('#ifndef %s' % ( include_guard ))
    gen_c('#define %s' % ( include_guard ))
    gen_c('')
    gen_banner()

def end_h_file():
    gen_c('#endif')

def start_c_file(name):
    gen_banner()

    gen_c('#include "qemu/osdep.h"')
    gen_c('')

def end_c_file():
    return

def gen_typedefs():
    start_h_file('typedefs')

    PublicInclude.gen()
    gen_c('')

    ExportedType.gen_pub_typedef_decls()

    ExportedFct.gen_pub_typedef_decls()

    gen_c('')
    end_h_file()

def gen_export_struct():
    start_h_file('exports')

    gen_c('#include "libqemu/exports/typedefs.h"')

    gen_c('typedef struct LibQemuExports LibQemuExports;')
    gen_c('')

    gen_c("struct LibQemuExports {")

    indent_push()

    for f in ExportedFct.fcts.values():
        gen_c('{typ} {name};'.format(
            typ = f.get_pub_typedef_name(),
            name = f.pub)
        )

    indent_pop()

    gen_c("};")
    gen_c('')

    end_h_file()

def gen_fill_fct():
    start_c_file('fill')
    gen_c('')

    gen_c('#include "libqemu/fill.h"')
    gen_c('#include "libqemu/exports/iothread-wrapper.h"')
    PrivateInclude.gen()
    gen_c('')

    gen_c('void libqemu_exports_fill(LibQemuExports *exports)')
    gen_c('{')

    indent_push()

    for f in ExportedFct.fcts.values():
        if f.arch:
            gen_c('#ifdef TARGET_{}'.format(f.arch.upper()))

        gen_c('exports->{name} = ({cast}) {target};'.format(
            name = f.pub,
            cast = f.get_pub_cast(),
            target = f.get_priv_target())
        )

        if f.arch:
            gen_c('#else')
            gen_c('exports->{name} = NULL;'.format(name = f.pub))
            gen_c('#endif')

    indent_pop()

    gen_c('}')

    end_c_file()

def gen_iothread_wrapped_param_struct(f):
    gen_c('struct {}_params {{'.format(f.priv))
    indent_push()

    if f.ret != 'void':
        gen_c('{} ret;'.format(f.ret))

    idx = 0
    for a in f.args:
        gen_c('{} p{};'.format(a, idx))
        idx += 1

    indent_pop()
    gen_c('};')

def gen_iothread_wrapped_worker(f):
    gen_c('static void {}_worker(void *opaque)'.format(f.priv))
    gen_c('{')
    indent_push()

    gen_c('struct {0}_params *params = (struct {0}_params *) opaque;'.format(f.priv))

    ret = ''
    if f.ret != 'void':
        ret = 'params->ret = '

    gen_c('{ret}{name}({args});'.format(
        ret = ret,
        name = f.priv,
        args = f.format_args(fmt = 'params->p{idx}')))

    indent_pop()
    gen_c('}')

def gen_wrapper_fct_def_begin(f):
    gen_c('{ret} {name}({args})'.format(
        ret = f.ret,
        name = f.get_priv_target(),
        args = f.format_args()))

    gen_c('{')
    indent_push()

def gen_wrapper_fct_def_end(f):
    indent_pop()
    gen_c('}')

def gen_iothread_wrapper(f):
    gen_wrapper_fct_def_begin(f)

    gen_c('struct {name}_params params;'.format(name = f.priv))
    gen_c('')

    for idx in range(len(f.args)):
        gen_c('params.p{0} = p{0};'.format(idx))
    gen_c('')

    gen_c('run_on_iothread({}_worker, &params);'.format(f.priv))

    if f.ret != 'void':
        gen_c('')
        gen_c('return params.ret;')

    gen_wrapper_fct_def_end(f)

def gen_on_iothread_wrapped_fct(f):
    gen_iothread_wrapped_param_struct(f)
    gen_c('')

    gen_iothread_wrapped_worker(f)
    gen_c('')

    gen_iothread_wrapper(f)
    gen_c('')

def gen_iothread_locked_wrapped_fct(f):
    gen_wrapper_fct_def_begin(f)

    gen_c('bool was_locked = qemu_mutex_iothread_locked();')

    gen_c('if (!was_locked) {')
    indent_push()
    gen_c('qemu_mutex_lock_iothread();')
    indent_pop()
    gen_c('}')
    gen_c('')

    ret = ''
    if f.ret != 'void':
        ret = '{} ret = '.format(f.ret)

    gen_c('{ret}{name}({args});'.format(
        ret = ret,
        name = f.priv,
        args = f.format_args(fmt = 'p{idx}')))
    gen_c('')

    gen_c('if (!was_locked) {')
    indent_push()
    gen_c('qemu_mutex_unlock_iothread();')
    indent_pop()
    gen_c('}')
    gen_c('')

    if f.ret != 'void':
        gen_c('return ret;')

    gen_wrapper_fct_def_end(f)

def gen_iothread_wrappers_c():
    start_c_file('iothread-wrapper')
    gen_c('')

    gen_c('#include "qemu/main-loop.h"')
    gen_c('#include "libqemu/exports/iothread-wrapper.h"')
    gen_c('#include "libqemu/run-on-iothread.h"')
    gen_c('')

    for f in ExportedFct.fcts.values():
        if f.on_iothread:
            gen_on_iothread_wrapped_fct(f)
        elif f.iothread_locked:
            gen_iothread_locked_wrapped_fct(f)

    end_c_file()

def gen_iothread_wrappers_h():
    start_h_file('iothread-wrapper')

    PrivateInclude.gen()
    PublicInclude.gen()

    for f in ExportedFct.fcts.values():
        if f.wrapped:
            gen_c('{ret} {name}({args});'.format(
                ret = f.ret,
                name = f.get_priv_target(),
                args = f.format_args())
            )

    gen_c('')

    end_h_file()

GEN_FCTS = {
        '--typedefs-h': gen_typedefs,
        '--struct-h': gen_export_struct,
        '--fill-c': gen_fill_fct,
        '--iothread-wrapper-c': gen_iothread_wrappers_c,
        '--iothread-wrapper-h': gen_iothread_wrappers_h,
}

if len(sys.argv) < 3:
    eprint('usage: %s <--gen-arg> <exports file>' % (sys.argv[0]))
    eprint('with <--gen-arg> being one of:')
    for k in GEN_FCTS.keys():
        eprint('  %s' % k)
    exit(1)

gen_fct = sys.argv[1]
export_file = sys.argv[2]

if not gen_fct in GEN_FCTS:
    eprint('Unknown argument %s' % gen_fct)
    exit(1)

with open(export_file) as f:
    code = compile(f.read(), export_file, 'exec')
    exec(code)

GEN_FCTS[gen_fct]()
