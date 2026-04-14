#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

rm -rf build/coverage_html
mkdir build/coverage_html
gcovr \
      --gcov-ignore-parse-errors suspicious_hits.warn \
      --gcov-ignore-parse-errors negative_hits.warn \
      --merge-mode-functions=separate \
      --html-details build/coverage_html/index.html \
      "$@"

echo file://$(pwd)/build/coverage_html/index.html
