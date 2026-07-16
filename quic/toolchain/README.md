# quic/toolchain

## Augment a Hexagon toolchain with Linux/musl support

The Hexagon SDK ships a baremetal-only toolchain; QEMU's
hexagon-linux-user TCG tests need a Linux/musl cross-compiler.

### Build

    export GITLAB_TOOLCHAIN_UPLOAD_TOKEN=<token>
    ./quic/toolchain/build.sh <revision> [<baremetal-ver>] [<musl-ver>]

Runs fetch, augment, pack, and upload under
/tmp/hexagon-toolchain-build.

  - <revision>: required; the upload revision (e.g. revision-1). Bump
    it on every re-upload so the download URL changes.
  - <baremetal-ver>: optional baremetal SDK version; defaults to
    21.0.03.
  - <musl-ver>: optional musl version; defaults to 22.1.4.

### Fetch

    ./quic/toolchain/fetch-hexagon-toolchains.sh [<baremetal-ver>] [<musl-ver>]

Defaults to baremetal 21.0.03 and musl 22.1.4 if no args given.

### Where to get the toolchains

  - Baremetal SDK: /pkg/qct/software/hexagon/releases/tools
  - Linux/musl reference: https://github.com/qualcomm/toolchain_for_hexagon

### Augment

    ./quic/toolchain/add-linux-musl-support.sh <dest-toolchain-root> <src-linux-toolchain>

  - <dest-toolchain-root>: path to the baremetal toolchain (e.g. .../21.0.03/Tools)
   - <src-linux-toolchain>: path to the reference hexagon-unknown-linux-musl
                            cross-toolchain host dir
                            (e.g. .../clang+llvm-22.1.4-cross-hexagon-unknown-linux-musl/x86_64-linux-gnu)

### Pack

    ./quic/toolchain/pack-hexagon-toolchain.sh <toolchain-dir>

Produces hexagon-toolchain-<basename>-extended.tar.zst in the current
directory, where <basename> is the basename of the toolchain dir.

### Upload

    export GITLAB_TOOLCHAIN_UPLOAD_TOKEN=<token>
    ./quic/toolchain/upload-hexagon-toolchain.sh <tarball> <revision>

### Use

  1. Choose a new revision such as `revision-1`; increment it (for
     example `revision-2`) whenever the packaged bytes or metadata
     change, even if the toolchain version is unchanged.
  2. Pass that revision to `build.sh` or `upload-hexagon-toolchain.sh`.
  3. Verify the artifact exists at the resulting authenticated
     download URL.
  4. Set `HEXAGON_TOOLCHAIN_REVISION` to the same value in
     `tests/docker/dockerfiles/debian-hexagon-cross.docker` (and bump
     `HEXAGON_TOOLCHAIN_VERSION` there only when the toolchain version
     itself changes), then rebuild the image.
