#!/bin/sh
set -eu

PROJECT_ROOT_PATH=$(cd "$(dirname "$0")/.." && pwd)

if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: docker is required but was not found in PATH." >&2
    exit 1
fi

cd "${PROJECT_ROOT_PATH}"
docker compose -f docker-compose.sophon.yml up --build
