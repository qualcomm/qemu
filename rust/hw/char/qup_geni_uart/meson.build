# SPDX-License-Identifier: GPL-2.0-or-later
_libqup_geni_uart_rs = static_library(
  'qup_geni_uart',
  files('src/lib.rs'),
  override_options: ['rust_std=2021', 'build.rust_std=2021'],
  rust_abi: 'rust',
  link_with: [
    _util_rs,
    _migration_rs,
    _bql_rs,
    _qom_rs,
    _chardev_rs,
    _system_rs,
    _hwcore_rs,
  ],
  dependencies: [
    bits_rs,
    common_rs,
    qemu_macros,
  ],
)

rust_devices_ss.add(when: 'CONFIG_X_QUP_GENI_UART_RUST', if_true: [declare_dependency(
  link_whole: [_libqup_geni_uart_rs],
  # Putting proc macro crates in `dependencies` is necessary for Meson to find
  # them when compiling the root per-target static rust lib.
  dependencies: [qemu_macros],
  variables: {'crate': 'qup_geni_uart'},
)])
