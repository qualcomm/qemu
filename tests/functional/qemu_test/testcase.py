# Test class and utilities for functional tests
#
# Copyright 2018, 2024 Red Hat, Inc.
#
# Original Author (Avocado-based tests):
#  Cleber Rosa <crosa@redhat.com>
#
# Adaption for standalone version:
#  Thomas Huth <thuth@redhat.com>
#
# This work is licensed under the terms of the GNU GPL, version 2 or
# later.  See the COPYING file in the top-level directory.

import logging
import os
from pathlib import Path
import shutil
from subprocess import run
import sys
import tempfile
import warnings
import unittest
import uuid

import pycotap

from qemu.machine import QEMUMachine
from qemu.utils import hvf_available, kvm_available, tcg_available

from .archive import archive_extract
from .asset import Asset
from .config import BUILD_DIR, dso_suffix
from .uncompress import uncompress


class QemuBaseTest(unittest.TestCase):

    def uncompress(self, compressed, target=None, format=None):
        '''
        @params compressed: filename, Asset, or file-like object to uncompress
        @params format: optional compression format (gzip, lzma)

        Uncompresses @compressed into the scratch directory.

        If @format is None, heuristics will be applied to guess the
        format from the filename or Asset URL. @format must be non-None
        if @uncompressed is a file-like object.

        Returns the fully qualified path to the uncompressed file
        '''
        self.log.debug(f"Uncompress {compressed} format={format}")
        if isinstance(compressed, Asset):
            compressed.fetch()

        if target is not None:
            uncompressed = self.scratch_file(target)
        else:
            (name, _ext) = os.path.splitext(str(compressed))
            uncompressed = self.scratch_file(os.path.basename(name))

        uncompress(compressed, uncompressed, format)

        return uncompressed

    def archive_extract(self, archive, format=None, sub_dir=None, member=None):
        '''
        @params archive: filename, Asset, or file-like object to extract
        @params format: optional archive format (tar, zip, deb, cpio)
        @params sub_dir: optional sub-directory to extract into
        @params member: optional member file to limit extraction to

        Extracts @archive into the scratch directory, or a directory beneath
        named by @sub_dir. All files are extracted unless @member specifies
        a limit.

        If @format is None, heuristics will be applied to guess the
        format from the filename or Asset URL. @format must be non-None
        if @archive is a file-like object.

        If @member is non-None, returns the fully qualified path to @member
        '''
        self.log.debug(f"Extract {archive} format={format}" +
                       f"sub_dir={sub_dir} member={member}")
        if isinstance(archive, Asset):
            archive.fetch()
        if sub_dir is None:
            archive_extract(archive, self.scratch_file(), format, member)
        else:
            archive_extract(archive, self.scratch_file(sub_dir),
                            format, member)

        if member is not None:
            return self.scratch_file(member)
        return None

    def socket_dir(self):
        '''
        Create a temporary directory suitable for storing UNIX
        socket paths.

        Returns: a tempfile.TemporaryDirectory instance
        '''
        if self.socketdir is None:
            self.socketdir = tempfile.TemporaryDirectory(
                prefix="qemu_func_test_sock_")
        return self.socketdir

    def data_file(self, *args):
        '''
        @params args list of zero or more subdirectories or file

        Construct a path for accessing a data file located
        relative to the source directory that is the root for
        functional tests.

        @args may be an empty list to reference the root dir
        itself, may be a single element to reference a file in
        the root directory, or may be multiple elements to
        reference a file nested below. The path components
        will be joined using the platform appropriate path
        separator.

        Returns: string representing a file path
        '''
        return str(Path(Path(__file__).parent.parent, *args))

    def build_file(self, *args):
        '''
        @params args list of zero or more subdirectories or file

        Construct a path for accessing a data file located
        relative to the build directory root.

        @args may be an empty list to reference the build dir
        itself, may be a single element to reference a file in
        the build directory, or may be multiple elements to
        reference a file nested below. The path components
        will be joined using the platform appropriate path
        separator.

        Returns: string representing a file path
        '''
        return str(Path(BUILD_DIR, *args))

    def scratch_file(self, *args):
        '''
        @params args list of zero or more subdirectories or file

        Construct a path for accessing/creating a scratch file
        located relative to a temporary directory dedicated to
        this test case. The directory and its contents will be
        purged upon completion of the test.

        @args may be an empty list to reference the scratch dir
        itself, may be a single element to reference a file in
        the scratch directory, or may be multiple elements to
        reference a file nested below. The path components
        will be joined using the platform appropriate path
        separator.

        Returns: string representing a file path
        '''
        return str(Path(self.workdir, *args))

    def log_file(self, *args):
        '''
        @params args list of zero or more subdirectories or file

        Construct a path for accessing/creating a log file
        located relative to a temporary directory dedicated to
        this test case. The directory and its log files will be
        preserved upon completion of the test.

        @args may be an empty list to reference the log dir
        itself, may be a single element to reference a file in
        the log directory, or may be multiple elements to
        reference a file nested below. The path components
        will be joined using the platform appropriate path
        separator.

        Returns: string representing a file path
        '''
        return str(Path(self.outputdir, *args))

    def plugin_file(self, plugin_name):
        '''
        @params plugin name

        Return the full path to the plugin taking into account any host OS
        specific suffixes.
        '''
        sfx = dso_suffix()
        return os.path.join('tests', 'tcg', 'plugins', f'{plugin_name}.{sfx}')

    def assets_available(self):
        for name, asset in vars(self.__class__).items():
            if name.startswith("ASSET_") and isinstance(asset, Asset):
                if not asset.available():
                    self.log.debug(f"Asset {asset.url} not available")
                    return False
        return True

    def setUp(self):
        self.qemu_bin = os.getenv('QEMU_TEST_QEMU_BINARY')
        self.assertIsNotNone(self.qemu_bin, 'QEMU_TEST_QEMU_BINARY must be set')
        self.arch = self.qemu_bin.split('-')[-1]
        self.socketdir = None

        self.outputdir = self.build_file('tests', 'functional',
                                         self.arch, self.id())
        self.workdir = os.path.join(self.outputdir, 'scratch')
        if os.path.exists(self.workdir):
            # Purge as safety net in case of unclean termination of
            # previous test, or use of QEMU_TEST_KEEP_SCRATCH
            shutil.rmtree(self.workdir)
        os.makedirs(self.workdir, exist_ok=True)

        self.log_filename = self.log_file('base.log')
        self.log = logging.getLogger('qemu-test')
        self.log.setLevel(logging.DEBUG)
        self._log_fh = logging.FileHandler(self.log_filename, mode='w')
        self._log_fh.setLevel(logging.DEBUG)
        file_formatter = logging.Formatter(
            '%(asctime)s - %(levelname)s: %(name)s.%(funcName)s %(message)s')
        self._log_fh.setFormatter(file_formatter)
        self.log.addHandler(self._log_fh)

        # Capture QEMUMachine logging
        self.machinelog = logging.getLogger('qemu.machine')
        self.machinelog.setLevel(logging.DEBUG)
        self.machinelog.addHandler(self._log_fh)
        self.qmplog = logging.getLogger('qemu.qmp')
        self.qmplog.setLevel(logging.DEBUG)
        self.qmplog.addHandler(self._log_fh)

        if not self.assets_available():
            self.skipTest('One or more assets is not available')

    def tearDown(self):
        if "QEMU_TEST_KEEP_SCRATCH" not in os.environ:
            shutil.rmtree(self.workdir)
        if self.socketdir is not None:
            self.socketdir.cleanup()
            self.socketdir = None
        self.qmplog.removeHandler(self._log_fh)
        self.machinelog.removeHandler(self._log_fh)
        self.log.removeHandler(self._log_fh)
        self._log_fh.close()

        # By now this test's VMs/helpers have been shut down. Anything still
        # alive is a leak attributable to this specific test and a likely
        # cause of a spurious meson TIMEOUT; report it (without force-exiting,
        # as further tests may still run).
        try:
            self._diagnose_pending_state(self.id())
        except Exception as ex:  # pylint: disable=broad-except
            print(f"hang diagnostics failed: {ex!r}", file=sys.stderr)

    @staticmethod
    def _diagnose_pending_state(context, arm_watchdog=False):
        '''
        Diagnostic for the "complete TAP output but meson reports TIMEOUT"
        problem.

        meson's TAP harness only considers a test finished once the process
        has exited *and* its stdout/stderr pipes have reached EOF, all within
        the test timeout (see mesonbuild/mtest.py: it awaits p.wait() together
        with the stdout/stderr reader tasks). Once a test's tearDown() has run,
        or once all tests are complete, there should be no live non-daemon
        threads and no surviving child processes. If any remain, they are what
        keeps the interpreter alive (a stuck non-daemon thread blocks CPython's
        shutdown) or keeps meson's output pipe open (a child process still
        holding an inherited fd 1/2), which meson counts as a timeout even
        though all TAP has already been written.

        @context: a label (e.g. a test id, or "post-run") identifying when
                  this check ran, so a leak can be attributed to a specific
                  test rather than only the module as a whole.
        @arm_watchdog: when True (only appropriate once all tests are done),
                  and a non-daemon thread is still running, arm a faulthandler
                  watchdog to dump every thread's stack and force-exit, so an
                  opaque timeout becomes an actionable backtrace. Between
                  tests this must stay False -- other tests still need to run.

        Anything suspicious is reported to stderr, which meson captures
        separately from the TAP stream on stdout.
        '''
        import threading

        out = sys.stderr

        def fd_target(path):
            try:
                return os.readlink(path)
            except OSError:
                return None

        live_threads = [t for t in threading.enumerate()
                        if t is not threading.main_thread() and t.is_alive()]
        nondaemon = [t for t in live_threads if not t.daemon]

        # Identify our stdout/stderr so we can spot any child still holding
        # them (a held pipe never reaches EOF -> meson waits the full timeout).
        my_stdout = fd_target("/proc/self/fd/1")
        my_stderr = fd_target("/proc/self/fd/2")

        children = []
        procdir = "/proc"
        if os.path.isdir(procdir):
            mypid = os.getpid()
            for entry in os.listdir(procdir):
                if not entry.isdigit():
                    continue
                try:
                    with open(f"{procdir}/{entry}/stat", "rb") as f:
                        stat = f.read().decode("latin1")
                    # Fields after the (possibly parenthesised) comm field:
                    # state ppid ...; rsplit(')') to survive ')' in comm.
                    ppid = int(stat.rsplit(")", 1)[1].split()[1])
                except (OSError, ValueError, IndexError):
                    continue
                if ppid != mypid:
                    continue
                try:
                    with open(f"{procdir}/{entry}/cmdline", "rb") as f:
                        cmd = f.read().replace(b"\0", b" ").decode(
                            errors="replace").strip()
                except OSError:
                    cmd = ""
                holds = []
                fddir = f"{procdir}/{entry}/fd"
                try:
                    for fd in os.listdir(fddir):
                        tgt = fd_target(f"{fddir}/{fd}")
                        if tgt is not None and tgt == my_stdout:
                            holds.append(f"our-stdout(fd={fd})")
                        elif tgt is not None and tgt == my_stderr:
                            holds.append(f"our-stderr(fd={fd})")
                except OSError:
                    pass
                children.append((int(entry), cmd or "?", holds))

        if not live_threads and not children:
            return

        print(f"\n=== qemu-test hang diagnostics [{context}] ===", file=out)
        print("Nothing should be left running at this point. The items below "
              "can delay process exit or keep meson's output pipe open, which "
              "meson reports as a TIMEOUT even once all TAP has been written.",
              file=out)

        if live_threads:
            print(f"Live threads besides main: {len(live_threads)} "
                  f"({len(nondaemon)} non-daemon)", file=out)
            for t in live_threads:
                print(f"  - name={t.name!r} daemon={t.daemon} "
                      f"ident={t.ident}", file=out)

        if children:
            print(f"Surviving child processes: {len(children)}", file=out)
            for pid, cmd, holds in children:
                extra = (" [" + ", ".join(holds) + "]") if holds else ""
                print(f"  - pid={pid}{extra} cmd={cmd!r}", file=out)

        out.flush()

        # A stuck non-daemon thread blocks interpreter shutdown outright: the
        # process never exits and meson waits the full timeout. Dump all
        # thread stacks after a short grace period and force-exit. Only do
        # this once all tests are done -- never between tests.
        if arm_watchdog and nondaemon:
            import faulthandler
            try:
                secs = int(os.environ.get("QEMU_TEST_HANG_TIMEOUT", "15"))
            except ValueError:
                secs = 15
            print(f"Non-daemon thread(s) still alive; arming faulthandler "
                  f"watchdog ({secs}s) to dump stacks and force-exit.",
                  file=out)
            out.flush()
            faulthandler.dump_traceback_later(secs, exit=True)

    @staticmethod
    def main():
        import faulthandler

        warnings.simplefilter("default")
        os.environ["PYTHONWARNINGS"] = "default"

        # Dump a traceback on fatal signals (e.g. if meson SIGKILLs us on
        # timeout after a preceding SIGTERM) to aid hang diagnosis.
        faulthandler.enable()

        test_module = os.path.basename(sys.argv[0])[:-3]

        cache = os.environ.get("QEMU_TEST_PRECACHE", None)
        if cache is not None:
            Asset.precache_suites(test_module, cache)
            return

        tr = pycotap.TAPTestRunner(message_log = pycotap.LogMode.LogToError,
                                   test_output_log = pycotap.LogMode.LogToError)
        res = unittest.main(test_module, testRunner = tr, exit = False)
        failed = {}
        for (test, _message) in res.result.errors + res.result.failures:
            if hasattr(test, "log_filename") and not test.id() in failed:
                print('More information on ' + test.id() + ' could be found here:'
                      '\n %s' % test.log_filename, file=sys.stderr)
                if hasattr(test, 'console_log_name'):
                    print(' %s' % test.console_log_name, file=sys.stderr)
                failed[test.id()] = True

        # All TAP output has now been emitted. If the process nonetheless
        # fails to exit promptly, meson will report a spurious TIMEOUT; try
        # to pinpoint why before handing control to sys.exit().
        try:
            QemuBaseTest._diagnose_pending_state("post-run",
                                                 arm_watchdog=True)
        except Exception as ex:  # pylint: disable=broad-except
            print(f"hang diagnostics failed: {ex!r}", file=sys.stderr)

        sys.exit(not res.result.wasSuccessful())


class QemuUserTest(QemuBaseTest):

    def setUp(self):
        super().setUp()
        self._ldpath = []

    def add_ldpath(self, ldpath):
        self._ldpath.append(os.path.abspath(ldpath))

    def run_cmd(self, bin_path, args=None):
        if args is None:
            args = []
        return run([self.qemu_bin]
                   + ["-L %s" % ldpath for ldpath in self._ldpath]
                   + [bin_path]
                   + args,
                   text=True, capture_output=True)

class QemuSystemTest(QemuBaseTest):
    """Facilitates system emulation tests."""

    cpu = None
    machine = None
    _machinehelp = None

    def setUp(self):
        self._vms = {}

        super().setUp()

        console_log = logging.getLogger('console')
        console_log.setLevel(logging.DEBUG)
        self.console_log_name = self.log_file('console.log')
        self._console_log_fh = logging.FileHandler(self.console_log_name,
                                                   mode='w')
        self._console_log_fh.setLevel(logging.DEBUG)
        file_formatter = logging.Formatter('%(asctime)s: %(message)s')
        self._console_log_fh.setFormatter(file_formatter)
        console_log.addHandler(self._console_log_fh)

    def set_machine(self, machinename):
        cls = type(self)

        if not hasattr(cls, "_machines"):
            tmp_vm = QEMUMachine(self.qemu_bin)
            tmp_vm.set_machine('none')

            try:
                tmp_vm.launch()
                resp = tmp_vm.qmp('query-machines')

                machines = resp.get('return', [])
                cls._machines = []
                for m in machines:
                    if 'name' in m:
                        cls._machines.append(m['name'])
                    if 'alias' in m:
                        cls._machines.append(m['alias'])

            finally:
                try:
                    tmp_vm.shutdown()
                except Exception:
                    pass

        self._machines = cls._machines

        if machinename not in self._machines:
            self.skipTest('no support for machine ' + machinename)

        self.machine = machinename

    def require_accelerator(self, accelerator):
        """
        Requires an accelerator to be available for the test to continue

        It takes into account the currently set qemu binary.

        If the check fails, the test is canceled.  If the check itself
        for the given accelerator is not available, the test is also
        canceled.

        :param accelerator: name of the accelerator, such as "kvm" or "tcg"
        :type accelerator: str
        """
        checker = {'tcg': tcg_available,
                   'kvm': kvm_available,
                   'hvf': hvf_available,
                  }.get(accelerator)
        if checker is None:
            self.skipTest("Don't know how to check for the presence "
                          "of accelerator %s" % accelerator)
        if not checker(qemu_bin=self.qemu_bin):
            self.skipTest("%s accelerator does not seem to be "
                          "available" % accelerator)

    def require_netdev(self, netdevname):
        helptxt = run([self.qemu_bin, '-M', 'none', '-netdev', 'help'],
                      capture_output=True, check=True, encoding='utf8').stdout
        if helptxt.find('\n' + netdevname + '\n') < 0:
            self.skipTest('no support for ' + netdevname + ' networking')

    def require_device(self, devicename):
        helptxt = run([self.qemu_bin, '-M', 'none', '-device', 'help'],
                   capture_output=True, check=True, encoding='utf8').stdout
        if helptxt.find(devicename) < 0:
            self.skipTest('no support for device ' + devicename)

    def _new_vm(self, name, *args):
        vm = QEMUMachine(self.qemu_bin,
                         name=name,
                         base_temp_dir=self.workdir,
                         log_dir=self.log_file())
        self.log.debug('QEMUMachine "%s" created', name)
        self.log.debug('QEMUMachine "%s" temp_dir: %s', name, vm.temp_dir)

        sockpath = os.environ.get("QEMU_TEST_QMP_BACKDOOR", None)
        if sockpath is not None:
            vm.add_args("-chardev",
                        f"socket,id=backdoor,path={sockpath},server=on,wait=off",
                        "-mon", "chardev=backdoor,mode=control")

        if args:
            vm.add_args(*args)
        return vm

    @property
    def vm(self):
        return self.get_vm(name='default')

    def get_vm(self, *args, name=None):
        if not name:
            name = str(uuid.uuid4())
        if self._vms.get(name) is None:
            self._vms[name] = self._new_vm(name, *args)
            if self.cpu is not None:
                self._vms[name].add_args('-cpu', self.cpu)
            if self.machine is not None:
                self._vms[name].set_machine(self.machine)
        return self._vms[name]

    def set_vm_arg(self, arg, value):
        """
        Set an argument to list of extra arguments to be given to the QEMU
        binary. If the argument already exists then its value is replaced.

        :param arg: the QEMU argument, such as "-cpu" in "-cpu host"
        :type arg: str
        :param value: the argument value, such as "host" in "-cpu host"
        :type value: str
        """
        if not arg or not value:
            return
        if arg not in self.vm.args:
            self.vm.args.extend([arg, value])
        else:
            idx = self.vm.args.index(arg) + 1
            if idx < len(self.vm.args):
                self.vm.args[idx] = value
            else:
                self.vm.args.append(value)

    def tearDown(self):
        for vm in self._vms.values():
            try:
                vm.shutdown()
            except Exception as ex:
                self.log.error("Failed to teardown VM: %s", ex)
        logging.getLogger('console').removeHandler(self._console_log_fh)
        self._console_log_fh.close()
        super().tearDown()
