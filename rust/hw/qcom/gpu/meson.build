subproject('bilge-0.2-rs', required: true)
subproject('bilge-impl-0.2-rs', required: true)

bilge_dep = dependency('bilge-0.2-rs')
bilge_impl_dep = dependency('bilge-impl-0.2-rs')

_gpu_rs = static_library(
  'gpu',
  files('src/lib.rs'),
  override_options: ['rust_std=2021', 'build.rust_std=2021'],
  rust_abi: 'rust',
  dependencies: [
    bilge_dep,
    bilge_impl_dep,
    qemu_api,
    qemu_api_macros,
  ],
)

rust_devices_ss.add(when: 'QCOM_GRAPHICS', if_true: [declare_dependency(
  link_whole: [_gpu_rs],
  # Putting proc macro crates in `dependencies` is necessary for Meson to find
  # them when compiling the root per-target static rust lib.
  dependencies: [qemu_api_macros],
  variables: {'crate': 'gpu'},
)])
