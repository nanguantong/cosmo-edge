#!/bin/sh
set -eu

PROJECT_ROOT_PATH=$(cd "$(dirname "$0")/.." && pwd)

if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: docker is required but was not found in PATH." >&2
    exit 1
fi

cd "${PROJECT_ROOT_PATH}"
# Check if 'docker compose' (V2) is available, otherwise use 'docker-compose' (V1)
if docker compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE_CMD="docker compose"
elif command -v docker-compose >/dev/null 2>&1; then
    DOCKER_COMPOSE_CMD="docker-compose"
else
    echo "ERROR: Neither 'docker compose' nor 'docker-compose' is installed." >&2
    exit 1
fi

${DOCKER_COMPOSE_CMD} -f docker-compose.sophon.yml up --build
