#!/usr/bin/env python3
"""
QEMU Hexagon Test Configuration Runner

This script tests multiple QEMU build configurations and test suites,
generating a comprehensive test report.
"""

import os
import sys
import subprocess
import time
import json
import shutil
import multiprocessing
import getpass
import logging
from datetime import datetime
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional, Tuple, Any
import argparse

@dataclass
class TestResult:
    """Container for test execution results"""
    name: str
    success: bool
    duration: float
    stdout: str
    stderr: str
    exit_code: int
    error_summary: Optional[str] = None

@dataclass
class ConfigResult:
    """Container for configuration test results"""
    config_name: str
    configure_success: bool
    configure_duration: float
    build_success: bool
    build_duration: float
    test_results: List[TestResult]
    configure_output: str = ""
    build_output: str = ""
    total_duration: float = 0.0

class QEMUTester:
    """Main test runner class"""

    def __init__(self, source_dir: Path, work_dir: Path, verbose: bool = False, job_multiplier: float = 0.75, tag_on_success: bool = False):
        self.source_dir = Path(source_dir).resolve()
        self.work_dir = Path(work_dir).resolve()
        self.verbose = verbose
        self.tag_on_success = tag_on_success
        self.results: List[ConfigResult] = []

        # Create work directory if it doesn't exist
        self.work_dir.mkdir(parents=True, exist_ok=True)

        # Set up log file
        self.log_file = self.work_dir / f"test_hex_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log"

        # Set up logging
        self.setup_logging()

        # Calculate number of parallel jobs based on CPU count
        cpu_count = multiprocessing.cpu_count()
        self.jobs = max(1, int(cpu_count * job_multiplier))
        logging.info(f"Using {self.jobs} parallel jobs (CPU count: {cpu_count}, multiplier: {job_multiplier})")
        logging.info(f"Log file: {self.log_file}")

        # Get git version information
        self.git_info = self.get_git_info()

        # Detect hexagon-softmmu support
        self.hexagon_softmmu_supported = self.detect_hexagon_softmmu_support()

        # Test configurations to run
        base_targets = "hexagon-linux-user,hexagon-softmmu"
        self.actual_targets = self.get_target_list(base_targets)

        self.configurations = [
            {
                "name": "hexagon-debug-plugins",
                "targets": self.actual_targets,
                "extra_flags": ["--enable-debug-tcg", "--enable-plugins"],
                "cc": "gcc",
            },
            {
                "name": "hexagon-clang-asan",
                "targets": self.actual_targets,
                "extra_flags": ['--enable-asan', '--enable-docs'],
                "cc": "clang",
            },
#           {
#               "name": "hexagon-i386-host",
#               "targets": self.actual_targets,
#               "extra_flags": ["--cpu=i686", ],
#               "cc": "gcc",
#           },
#           {
#               "name": "hexagon-noidef",
#               "targets": self.actual_targets,
#               "extra_flags": ["--enable-debug-tcg", "--disable-hexagon-idef-parser"],
#               "cc": "gcc",
#           },
        ]

        check_func = ['check-functional'] if self.hexagon_softmmu_supported else []
        # Test suites to run for each configuration
        self.test_suites = [
            {"name": "check-functional-thorough", "command": ["make", "check", "SPEED=thorough", "-j4"] + check_func, "timeout": 3600},
            {"name": "check-tcg", "command": ["make", "check-tcg", "-j4"], "timeout": 1800},
        ]

    def setup_logging(self):
        """Set up logging configuration"""
        # Create logger
        logger = logging.getLogger()
        logger.setLevel(logging.DEBUG)

        # Clear any existing handlers
        logger.handlers = []

        # Create formatter
        formatter = logging.Formatter('[%(asctime)s] %(levelname)s: %(message)s',
                                    datefmt='%H:%M:%S')

        # Console handler
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setLevel(logging.DEBUG if self.verbose else logging.INFO)
        console_handler.setFormatter(formatter)
        logger.addHandler(console_handler)

        # File handler
        try:
            file_handler = logging.FileHandler(self.log_file, encoding='utf-8')
            file_handler.setLevel(logging.DEBUG)
            file_handler.setFormatter(formatter)
            logger.addHandler(file_handler)
        except Exception as e:
            logging.warning(f"Failed to set up file logging: {e}")

    def get_last_lines(self, text: str, n: int = 10) -> str:
        """Get the last N lines from text output"""
        if not text:
            return ""
        lines = text.strip().split('\n')
        last_lines = lines[-n:] if len(lines) > n else lines
        return '\n'.join(last_lines)

    def get_command_output_filename(self, cmd: List[str], config_name: str = None, suffix: str = "") -> Path:
        """Generate a filename for command output"""
        # Create a safe filename from the command
        cmd_name = '_'.join(cmd[:2])  # Use first two parts of command
        cmd_name = cmd_name.replace('/', '_').replace('-', '_')

        # Add timestamp
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')

        # Build filename
        if config_name:
            filename = f"{config_name}_{cmd_name}_{timestamp}{suffix}.log"
        else:
            filename = f"{cmd_name}_{timestamp}{suffix}.log"

        return self.work_dir / filename

    def get_git_info(self) -> Dict[str, str]:
        """Get git version and status information"""
        git_info = {
            "commit_hash": "unknown",
            "commit_date": "unknown",
            "branch": "unknown",
            "is_dirty": "unknown",
            "describe": "unknown",
        }

        try:
            # Get commit hash
            result = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                cwd=self.source_dir,
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                git_info["commit_hash"] = result.stdout.strip()

            # Get commit date
            result = subprocess.run(
                ["git", "show", "-s", "--format=%ci", "HEAD"],
                cwd=self.source_dir,
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                git_info["commit_date"] = result.stdout.strip()

            # Get current branch
            result = subprocess.run(
                ["git", "rev-parse", "--abbrev-ref", "HEAD"],
                cwd=self.source_dir,
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                git_info["branch"] = result.stdout.strip()

            # Check if repo is dirty (has uncommitted changes)
            result = subprocess.run(
                ["git", "status", "--porcelain"],
                cwd=self.source_dir,
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                git_info["is_dirty"] = "true" if result.stdout.strip() else "false"

            # Get git describe (version tag info)
            result = subprocess.run(
                ["git", "describe", "--tags", "--dirty", "--always"],
                cwd=self.source_dir,
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                git_info["describe"] = result.stdout.strip()

        except Exception as e:
            logging.warning(f"Failed to get git information: {e}")

        logging.info(f"Git info: {git_info['describe']} ({git_info['branch']}) {'(dirty)' if git_info['is_dirty'] == 'true' else ''}")
        return git_info

    def detect_hexagon_softmmu_support(self) -> bool:
        """Detect if hexagon-softmmu is supported in current version"""
        # Check if hexagon virt machine files exist
        virt_machine_file = self.source_dir / "hw" / "hexagon" / "virt.c"
        if not virt_machine_file.exists():
            logging.info("hexagon-softmmu not supported: hw/hexagon/virt.c not found")
            return False

        # Check if hexagon is in the list of supported system targets
        try:
            # Look for hexagon in the meson build system
            meson_build = self.source_dir / "meson.build"
            if meson_build.exists():
                with open(meson_build, 'r') as f:
                    content = f.read()
                    if 'hexagon-softmmu' in content:
                        logging.info("hexagon-softmmu supported: found in meson.build")
                        return True

            # Check if hexagon machine is defined in hw/Kconfig
            hw_kconfig = self.source_dir / "hw" / "Kconfig"
            if hw_kconfig.exists():
                with open(hw_kconfig, 'r') as f:
                    content = f.read()
                    if 'HEXAGON' in content or 'hexagon' in content:
                        logging.info("hexagon-softmmu supported: found in hw/Kconfig")
                        return True

            # Check configure script for hexagon-softmmu target
            configure_script = self.source_dir / "configure"
            if configure_script.exists():
                with open(configure_script, 'r') as f:
                    content = f.read()
                    if 'hexagon-softmmu' in content:
                        logging.info("hexagon-softmmu supported: found in configure script")
                        return True

            # Final check: look for hexagon machine definitions
            hex_machine_dir = self.source_dir / "hw" / "hexagon"
            if hex_machine_dir.exists():
                machine_files = list(hex_machine_dir.glob("*.c"))
                if machine_files:
                    logging.info(f"hexagon-softmmu supported: found machine files {[f.name for f in machine_files]}")
                    return True

        except Exception as e:
            logging.warning(f"Error detecting hexagon-softmmu support: {e}")

        logging.info("hexagon-softmmu not supported in this version")
        return False

    def get_target_list(self, base_targets: str) -> str:
        """Get target list, conditionally including hexagon-softmmu"""
        targets = base_targets.split(',')

        if 'hexagon-softmmu' in targets and not self.hexagon_softmmu_supported:
            # Remove hexagon-softmmu if not supported
            targets = [t for t in targets if t != 'hexagon-softmmu']
            logging.info("Removed hexagon-softmmu from target list (not supported)")

        return ','.join(targets)

    def run_command(self, cmd: List[str], cwd: Path, timeout: int = 300, config_name: str = None) -> Tuple[bool, str, str, int, float]:
        """Run a command and return success, stdout, stderr, exit_code, duration"""
        start_time = time.time()

        # Generate output filename
        output_file = self.get_command_output_filename(cmd, config_name)

        logging.info(f"Running: {' '.join(cmd)} (timeout: {timeout}s)")
        logging.debug(f"Command output will be saved to: {output_file}")

        try:
            result = subprocess.run(
                cmd,
                cwd=cwd,
                capture_output=True,
                text=True,
                timeout=timeout,
            )

            duration = time.time() - start_time
            success = result.returncode == 0

            # Save command output to file
            try:
                with open(output_file, 'w', encoding='utf-8') as f:
                    f.write(f"Command: {' '.join(cmd)}\n")
                    f.write(f"Working directory: {cwd}\n")
                    f.write(f"Exit code: {result.returncode}\n")
                    f.write(f"Duration: {duration:.2f}s\n")
                    f.write(f"Timestamp: {datetime.now().isoformat()}\n")
                    f.write("=" * 80 + "\n")
                    if result.stdout:
                        f.write("STDOUT:\n")
                        f.write(result.stdout)
                        f.write("\n" + "=" * 80 + "\n")
                    if result.stderr:
                        f.write("STDERR:\n")
                        f.write(result.stderr)
                        f.write("\n" + "=" * 80 + "\n")
            except Exception as e:
                logging.warning(f"Failed to write command output to {output_file}: {e}")

            if self.verbose:
                logging.debug(f"Command completed in {duration:.1f}s with exit code {result.returncode}")

            # Log last lines of output for failed commands
            if not success:
                combined_output = result.stdout + result.stderr
                if combined_output.strip():
                    last_lines = self.get_last_lines(combined_output, 10)
                    logging.error(f"Last 10 lines of output:\n{last_lines}")
                logging.error(f"Full command output saved to: {output_file}")

            return success, result.stdout, result.stderr, result.returncode, duration

        except subprocess.TimeoutExpired:
            duration = time.time() - start_time
            logging.warning(f"Command timed out after {timeout}s")

            # Save timeout information to file
            try:
                with open(output_file, 'w', encoding='utf-8') as f:
                    f.write(f"Command: {' '.join(cmd)}\n")
                    f.write(f"Working directory: {cwd}\n")
                    f.write(f"Exit code: TIMEOUT\n")
                    f.write(f"Duration: {duration:.2f}s (timed out)\n")
                    f.write(f"Timestamp: {datetime.now().isoformat()}\n")
                    f.write("=" * 80 + "\n")
                    f.write(f"Command timed out after {timeout} seconds\n")
            except Exception as e:
                logging.warning(f"Failed to write timeout info to {output_file}: {e}")

            return False, "", f"Command timed out after {timeout}s", -1, duration

        except Exception as e:
            duration = time.time() - start_time
            logging.error(f"Command failed with exception: {e}")

            # Save exception information to file
            try:
                with open(output_file, 'w', encoding='utf-8') as f:
                    f.write(f"Command: {' '.join(cmd)}\n")
                    f.write(f"Working directory: {cwd}\n")
                    f.write(f"Exit code: EXCEPTION\n")
                    f.write(f"Duration: {duration:.2f}s\n")
                    f.write(f"Timestamp: {datetime.now().isoformat()}\n")
                    f.write("=" * 80 + "\n")
                    f.write(f"Exception: {str(e)}\n")
            except Exception as write_e:
                logging.warning(f"Failed to write exception info to {output_file}: {write_e}")

            return False, "", str(e), -1, duration

    def clean_build_dir(self, build_dir: Path):
        """Clean the build directory"""
        if build_dir.exists():
            logging.info(f"Cleaning build directory: {build_dir}")
            shutil.rmtree(build_dir)
        build_dir.mkdir(parents=True, exist_ok=True)

    def configure_qemu(self, config: Dict[str, Any], build_dir: Path) -> Tuple[bool, str, float]:
        """Configure QEMU with the given configuration"""
        configure_script = self.source_dir / "configure"
        configure_cmd = [str(configure_script), f"--target-list={config['targets']}"]

        # Add extra flags
        configure_cmd.extend(config["extra_flags"])

        # Set compiler if specified
        env = os.environ.copy()
        if config["cc"] == "clang":
            env["CC"] = "clang"
            env["CXX"] = "clang++"

        logging.info(f"Configuring with: {' '.join(configure_cmd)}")

        # Generate output filename for configure
        output_file = self.get_command_output_filename(configure_cmd, config['name'], "_configure")

        start_time = time.time()
        try:
            result = subprocess.run(
                configure_cmd,
                cwd=build_dir,
                capture_output=True,
                text=True,
                timeout=300,
                env=env,
            )
            duration = time.time() - start_time

            success = result.returncode == 0
            output = result.stdout + "\n" + result.stderr

            # Save configure output to file
            try:
                with open(output_file, 'w', encoding='utf-8') as f:
                    f.write(f"Command: {' '.join(configure_cmd)}\n")
                    f.write(f"Working directory: {build_dir}\n")
                    f.write(f"Environment: CC={env.get('CC', 'default')}, CXX={env.get('CXX', 'default')}\n")
                    f.write(f"Exit code: {result.returncode}\n")
                    f.write(f"Duration: {duration:.2f}s\n")
                    f.write(f"Timestamp: {datetime.now().isoformat()}\n")
                    f.write("=" * 80 + "\n")
                    if result.stdout:
                        f.write("STDOUT:\n")
                        f.write(result.stdout)
                        f.write("\n" + "=" * 80 + "\n")
                    if result.stderr:
                        f.write("STDERR:\n")
                        f.write(result.stderr)
                        f.write("\n" + "=" * 80 + "\n")
            except Exception as e:
                logging.warning(f"Failed to write configure output to {output_file}: {e}")

            if not success:
                logging.error(f"Configure failed with exit code {result.returncode}")
                # Log last lines of configure output for debugging
                if output.strip():
                    last_lines = self.get_last_lines(output, 10)
                    logging.error(f"Last 10 lines of configure output:\n{last_lines}")
                logging.error(f"Full configure output saved to: {output_file}")

            return success, output, duration

        except Exception as e:
            duration = time.time() - start_time
            logging.error(f"Configure failed with exception: {e}")

            # Save exception information to file
            try:
                with open(output_file, 'w', encoding='utf-8') as f:
                    f.write(f"Command: {' '.join(configure_cmd)}\n")
                    f.write(f"Working directory: {build_dir}\n")
                    f.write(f"Environment: CC={env.get('CC', 'default')}, CXX={env.get('CXX', 'default')}\n")
                    f.write(f"Exit code: EXCEPTION\n")
                    f.write(f"Duration: {duration:.2f}s\n")
                    f.write(f"Timestamp: {datetime.now().isoformat()}\n")
                    f.write("=" * 80 + "\n")
                    f.write(f"Exception: {str(e)}\n")
            except Exception as write_e:
                logging.warning(f"Failed to write exception info to {output_file}: {write_e}")

            return False, str(e), duration

    def build_qemu(self, build_dir: Path, config_name: str) -> Tuple[bool, str, float]:
        """Build QEMU"""
        build_cmd = ["make", f"-j{self.jobs}"]

        logging.info(f"Building QEMU with {self.jobs} parallel jobs")

        success, stdout, stderr, exit_code, duration = self.run_command(
            build_cmd,
            build_dir,
            timeout=1800,
            config_name=config_name
        )

        output = stdout + "\n" + stderr

        if not success:
            logging.error(f"Build failed with exit code {exit_code}")
            # Note: Full output already saved to file by run_command

        return success, output, duration

    def run_test_suite(self, test_suite: Dict[str, Any], build_dir: Path, config_name: str) -> TestResult:
        """Run a single test suite"""
        logging.info(f"Running test suite: {test_suite['name']}")

        success, stdout, stderr, exit_code, duration = self.run_command(
            test_suite["command"],
            build_dir,
            timeout=test_suite["timeout"],
            config_name=f"{config_name}_{test_suite['name']}"
        )

        # Extract error summary for failed tests
        error_summary = None
        if not success:
            error_lines = stderr.split('\n')
            # Look for common error patterns
            for line in error_lines[-20:]:  # Last 20 lines
                if any(keyword in line.lower() for keyword in ["error", "fail", "abort", "segfault"]):
                    error_summary = line.strip()
                    break

            if not error_summary and stderr:
                error_summary = stderr.split('\n')[-1].strip()

        result = TestResult(
            name=test_suite['name'],
            success=success,
            duration=duration,
            stdout=stdout,
            stderr=stderr,
            exit_code=exit_code,
            error_summary=error_summary
        )

        status = "PASS" if success else "FAIL"
        logging.info(f"Test {test_suite['name']}: {status} ({duration:.1f}s)")

        # Log last lines of output for failed tests
        if not success:
            # Note: Full output already saved to file by run_command
            pass

        return result

    def test_configuration(self, config: Dict[str, Any]) -> ConfigResult:
        """Test a single configuration"""
        logging.info(f"Testing configuration: {config['name']}")

        build_dir = self.work_dir / f"build_{config['name']}"
        config_start_time = time.time()

        # Clean and prepare build directory
        self.clean_build_dir(build_dir)

        # Configure QEMU
        configure_success, configure_output, configure_duration = self.configure_qemu(config, build_dir)

        build_success = False
        build_output = ""
        build_duration = 0.0
        test_results = []

        if configure_success:
            # Build QEMU
            build_success, build_output, build_duration = self.build_qemu(build_dir, config['name'])

            if build_success:
                # Run test suites
                for test_suite in self.test_suites:
                    test_result = self.run_test_suite(test_suite, build_dir, config['name'])
                    test_results.append(test_result)
            else:
                logging.warning(f"Skipping tests for {config['name']} due to build failure")
        else:
            logging.warning(f"Skipping build and tests for {config['name']} due to configure failure")

        total_duration = time.time() - config_start_time

        result = ConfigResult(
            config_name=config['name'],
            configure_success=configure_success,
            configure_duration=configure_duration,
            build_success=build_success,
            build_duration=build_duration,
            test_results=test_results,
            configure_output=configure_output,
            build_output=build_output,
            total_duration=total_duration
        )

        logging.info(f"Configuration {config['name']} completed in {total_duration:.1f}s")
        return result

    def run_all_tests(self) -> List[ConfigResult]:
        """Run all test configurations"""
        logging.info("Starting QEMU test configuration runner")
        logging.info(f"Source directory: {self.source_dir}")
        logging.info(f"Work directory: {self.work_dir}")

        overall_start_time = time.time()

        for config in self.configurations:
            try:
                result = self.test_configuration(config)
                self.results.append(result)
            except Exception as e:
                logging.error(f"Configuration {config['name']} failed with exception: {e}")
                # Create a failed result
                failed_result = ConfigResult(
                    config_name=str(config['name']),
                    configure_success=False,
                    configure_duration=0,
                    build_success=False,
                    build_duration=0,
                    test_results=[],
                    configure_output=str(e),
                    total_duration=0
                )
                self.results.append(failed_result)

        overall_duration = time.time() - overall_start_time
        logging.info(f"All tests completed in {overall_duration:.1f}s")

        return self.results

    def generate_report(self, output_file: Path):
        """Generate a comprehensive test report"""
        logging.info(f"Generating test report: {output_file}")

        # Calculate summary statistics
        total_configs = len(self.results)
        successful_configs = sum(1 for r in self.results if r.configure_success and r.build_success)
        total_tests = sum(len(r.test_results) for r in self.results)
        successful_tests = sum(1 for r in self.results for t in r.test_results if t.success)

        report_data = {
            "generated_at": datetime.now().isoformat(),
            "log_file": str(self.log_file),
            "git_info": self.git_info,
            "target_info": {
                "hexagon_softmmu_supported": self.hexagon_softmmu_supported,
                "tested_targets": self.actual_targets.split(','),
            },
            "summary": {
                "total_configurations": total_configs,
                "successful_configurations": successful_configs,
                "total_tests": total_tests,
                "successful_tests": successful_tests,
                "overall_success_rate": f"{(successful_tests/total_tests*100):.1f}%" if total_tests > 0 else "0%",
            },
            "configurations": [asdict(result) for result in self.results],
        }

        # Write JSON report
        with open(output_file, 'w') as f:
            json.dump(report_data, f, indent=2)

        # Generate human-readable summary
        summary_file = output_file.with_suffix('.txt')

        # Calculate success rate
        success_rate = f"{(successful_tests/total_tests*100):.1f}%" if total_tests > 0 else "0%"

        # Build configuration details
        config_details = []
        for result in self.results:
            tests_section = ""
            if result.test_results:
                test_lines = []
                for test in result.test_results:
                    status = "PASS" if test.success else "FAIL"
                    test_line = f"  {test.name}: {status} ({test.duration:.1f}s)"
                    if not test.success and test.error_summary:
                        test_line += f"\n    Error: {test.error_summary}"
                    test_lines.append(test_line)
                tests_section = "Tests:\n" + "\n".join(test_lines)
            else:
                tests_section = "Tests: SKIPPED (build failed)"

            config_detail = f"""CONFIGURATION: {result.config_name}
{'-' * 40}
Configure: {'PASS' if result.configure_success else 'FAIL'} ({result.configure_duration:.1f}s)
Build: {'PASS' if result.build_success else 'FAIL'} ({result.build_duration:.1f}s)
{tests_section}
Total Duration: {result.total_duration:.1f}s"""

            config_details.append(config_detail)

        # Format git info for display
        git_summary = f"{self.git_info['describe']} ({self.git_info['branch']})"
        if self.git_info['is_dirty'] == 'true':
            git_summary += " [DIRTY]"

        # Add target support information
        target_info = f"Targets: {self.actual_targets}"
        if not self.hexagon_softmmu_supported:
            target_info += " (hexagon-softmmu excluded - not supported)"

        summary_content = f"""QEMU Hexagon Test Configuration Report
{'=' * 50}

Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

GIT INFORMATION
{'-' * 20}
Version: {git_summary}
Commit: {self.git_info['commit_hash'][:12]}
Date: {self.git_info['commit_date']}
Status: {'Modified (dirty)' if self.git_info['is_dirty'] == 'true' else 'Clean'}

TARGET INFORMATION
{'-' * 20}
{target_info}
Hexagon-softmmu Support: {'Yes' if self.hexagon_softmmu_supported else 'No'}

SUMMARY
{'-' * 20}
Total Configurations: {total_configs}
Successful Builds: {successful_configs}/{total_configs}
Total Tests: {successful_tests}/{total_tests}
Overall Success Rate: {success_rate}

{chr(10).join(config_details)}
"""

        with open(summary_file, 'w') as f:
            f.write(summary_content)

        logging.info(f"Reports generated:")
        logging.info(f"  JSON: {output_file}")
        logging.info(f"  Summary: {summary_file}")
        logging.info(f"  Log: {self.log_file}")

    def create_success_tag(self) -> bool:
        """Create a git tag on successful test completion"""
        if not self.tag_on_success:
            return False

        # Check if all tests were successful
        total_tests = sum(len(r.test_results) for r in self.results)
        successful_tests = sum(1 for r in self.results for t in r.test_results if t.success)
        all_configs_successful = all(r.configure_success and r.build_success for r in self.results)

        if total_tests == 0 or successful_tests < total_tests or not all_configs_successful:
            logging.info("Not creating tag: some tests failed or no tests were run")
            return False

        # Generate tag name
        username = getpass.getuser()
        datestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        tag_name = f"{username}-hexagon-tested-{datestamp}"

        # Generate tag annotation (short summary)
        successful_configs = sum(1 for r in self.results if r.configure_success and r.build_success)
        total_duration = sum(r.total_duration for r in self.results)

        # Create configuration summary
        config_lines = []
        for config_result in self.results:
            test_count = len(config_result.test_results)
            passed_tests = sum(1 for t in config_result.test_results if t.success)
            config_lines.append(f"  {config_result.config_name}: {passed_tests}/{test_count} tests ({config_result.total_duration:.1f}s)")

        annotation = f"""QEMU Hexagon Test Success - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

Git: {self.git_info['describe']} ({self.git_info['branch']})
Status: {'DIRTY' if self.git_info['is_dirty'] == 'true' else 'CLEAN'}

Test Summary:
- Configurations: {successful_configs}/{len(self.results)} passed
- Total Tests: {successful_tests}/{total_tests} passed
- Duration: {total_duration:.1f}s

Configurations:
{chr(10).join(config_lines)}

All tests passed successfully."""

        try:
            # Create annotated tag
            git_result = subprocess.run(
                ["git", "tag", "-a", tag_name, "-m", annotation],
                cwd=self.source_dir,
                capture_output=True,
                text=True,
                timeout=30,
            )

            if git_result.returncode == 0:
                logging.info(f"Created success tag: {tag_name}")
                return True
            else:
                logging.warning(f"Failed to create tag: {git_result.stderr}")
                return False

        except Exception as e:
            logging.warning(f"Failed to create tag: {e}")
            return False

def main():
    parser = argparse.ArgumentParser(
        description="QEMU Hexagon test configuration runner",
        epilog="Log files are automatically written to the work directory with timestamp."
    )
    parser.add_argument("--source-dir", "-s", default=".", help="QEMU source directory")
    parser.add_argument("--work-dir", "-w", default="./test_builds", help="Work directory for builds")

    # Generate default output filename in tag format
    username = getpass.getuser()
    datestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    default_output = f"{username}-hexagon-tested-{datestamp}.json"

    parser.add_argument("--output", "-o", default=default_output, help="Output report file")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", "-j", type=float, default=0.75,
                        help="Job multiplier for parallel builds (default: 0.75 * CPU count)")
    parser.add_argument("--tag-on-success", action="store_true",
                        help="Create a git tag on successful test completion")

    args = parser.parse_args()

    # Convert to absolute paths
    source_dir = Path(args.source_dir).resolve()
    work_dir = Path(args.work_dir).resolve()
    output_file = Path(args.output).resolve()

    # Validate source directory
    if not (source_dir / "configure").exists():
        print(f"ERROR: {source_dir} does not appear to be a QEMU source directory (no configure script found)")
        sys.exit(1)

    # Create work directory
    work_dir.mkdir(parents=True, exist_ok=True)

    # Run tests
    tester = QEMUTester(source_dir, work_dir, verbose=args.verbose, job_multiplier=args.jobs, tag_on_success=args.tag_on_success)
    results = tester.run_all_tests()

    # Generate report
    tester.generate_report(output_file)

    # Create success tag if requested and all tests passed
    tester.create_success_tag()

    # Print summary
    total_tests = sum(len(r.test_results) for r in results)
    successful_tests = sum(1 for r in results for t in r.test_results if t.success)

    print("\nTEST SUMMARY")
    print("=" * 40)
    print(f"Configurations: {len(results)}")
    print(f"Tests: {successful_tests}/{total_tests}")
    print(f"Success Rate: {(successful_tests/total_tests*100):.1f}%" if total_tests > 0 else "Success Rate: 0%")

    # Exit with error code if any tests failed
    if total_tests == 0 or successful_tests < total_tests:
        sys.exit(1)

if __name__ == "__main__":
    main()
