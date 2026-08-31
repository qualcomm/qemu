# Hexagon toolchain

To build and upload a Hexagon toolchain package, run the following commands
from this directory:

```sh
export GITLAB_TOOLCHAIN_UPLOAD_TOKEN=<token>
./build.sh
```

Each script can also be run directly.

`GITLAB_TOOLCHAIN_UPLOAD_TOKEN` is required to upload the package to the
GitLab Package Registry.

## Versions and revisions

 `versions.sh` defines the baremetal and Linux/musl toolchain versions, the
 package name, and the package revision. Update `versions.sh` before building a
 package for different toolchain versions or a different revision.
