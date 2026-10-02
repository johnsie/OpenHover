#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "Run this installer with sudo." >&2
  exit 1
fi

package_path=${1:?Usage: install-race-server.sh <openhover-raceserver.deb>}
if [[ ! -f ${package_path} ]]; then
  echo "Race server package is missing: ${package_path}" >&2
  exit 1
fi

dpkg --install "${package_path}"