#!/usr/bin/env bash

set -euxo pipefail

ci() {
	local phase; phase="$1"

	local paths
	local line; while read -r line; do
		paths+=("${line}")
	done < <(find src -xdev -not \( -path "*/.*" -prune \) -type f -executable -name "ci.sh" -print)

	local path
	for path in "${paths[@]}"; do
		# shellcheck disable=SC1090
		(. "${path}" "$@")
	done
}

ci "$@"
