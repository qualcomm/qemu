#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

./generate_coverage.sh --filter 'hw/arm/smmu*'
