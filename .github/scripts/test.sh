#!/usr/bin/env bash
set -euo pipefail
runtime=${1:-vm}

flutter pub get
(cd example && flutter pub get && bash "$MP_BUILD_SCRIPTS/test.sh" flutter "$runtime")
