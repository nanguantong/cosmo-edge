#!/bin/sh
# Called by build_sophon_package.ps1 inside a Docker container to populate the
# Sophon build volume from a Windows-host bind mount.
set -eu

cp -a /src/. /workspace/

# Remove directories we never need inside the build container, especially
# node_modules/ which contains Windows-native binaries (esbuild etc.) that
# would break the Linux builds.
for dir in build build_cpu build_cpu_verify build_cpu_windows build_output cmake-build-debug cmake-build-release .git node_modules src/web/node_modules; do
    rm -rf /workspace/$dir 2>/dev/null || true
done

echo "Source sync complete"
