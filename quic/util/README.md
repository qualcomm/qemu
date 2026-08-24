Files in this directory are intended to be sourced from other shell scripts.

    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
    readonly SCRIPT_DIR
    . "${SCRIPT_DIR}/util/foo.sh"

See each helper's header for its caller contract.
