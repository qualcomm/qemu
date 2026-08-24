Scripts in this directory and its subdirectories are CI-unaware: they must not
require CI-provided environment variables such as `CI_*` or `QUIC_*`. Pass
inputs via command-line options or use local defaults so the same entry points
can run from a developer checkout, a container, or GitLab CI.

CI-aware scripts live in [`quic-gitlab-ci.d/`](../quic-gitlab-ci.d/README.md).

