#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

if [ $# -eq 0 ]; then
    echo "usage: job"
    yq '.jobs | keys' < .github/workflows/build.yml
    exit 0
elif [ $# -ge 2 ]; then
    echo "usage: job"
    exit 1
fi

job=$1

cmd()
{
    yq '.jobs."'$job'".steps[].run' < .github/workflows/build.yml |
        grep -v null |
        grep -v sudo |
        sed -e 's/--pull newer //' -e 's/\\n//' -e 's/^"//' -e 's/"$//'
}

cmd
echo "------------------------------------------"

run=$(mktemp)
trap "rm -rf $run" EXIT

chmod +x $run
cat > $run << EOF
#!/usr/bin/env bash
set -euo pipefail
set -x
$(cmd)
EOF

$run
