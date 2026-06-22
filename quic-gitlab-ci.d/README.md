Scripts in this directory and its subdirectories are CI-aware: they are
permitted to read CI-provided environment variables such as `CI_*` and `QUIC_*`
directly. This is permission, not a requirement. Simple scripts may still avoid
CI variables where they do not need them.

CI-unaware scripts live in [`quic/`](../quic/README.md).

## Variable naming

CI variables defined by this project use the `QUIC_*` prefix to distinguish
them from upstream-provided ones such as `CI_*` (GitLab) or `DOCKER_*` (Docker
tooling). Use this prefix when introducing new project-defined variables.
