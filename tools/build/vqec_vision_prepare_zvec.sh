#!/usr/bin/env bash
# Fetch the reviewed public target SDK; no host C++ build is performed.
set -euo pipefail
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
dependency_root="$project_root/third_party/zvec"
if [[ -e "$dependency_root/sdk" ]]; then
    echo "Zvec SDK already exists; remove it explicitly before a reviewed replacement."
    exit 1
fi
staging="$(mktemp -d "$dependency_root/.download.XXXXXX")"
trap 'rm -rf "$staging"' EXIT
mapfile -t metadata < <(python3 - "$dependency_root/dependency.json" <<'PYTHON'
import json, sys
data = json.load(open(sys.argv[1]))
print(data["sdk_url"])
print(data["sdk_sha256"])
PYTHON
)
curl --fail --location --silent --show-error "${metadata[0]}" -o "$staging/sdk.tar.gz"
printf '%s  %s\n' "${metadata[1]}" "$staging/sdk.tar.gz" | sha256sum --check -
mkdir "$staging/sdk"
tar -xzf "$staging/sdk.tar.gz" -C "$staging/sdk"
test -f "$staging/sdk/include/zvec/c_api.h"
test -f "$staging/sdk/lib/libzvec_c_api.so"
mv "$staging/sdk" "$dependency_root/sdk"
echo "Pinned Zvec Linux AArch64 SDK installed."
