---
title: Troubleshooting
description: Common build, runtime, port, Sophon image, log, and documentation-site issues.
prev:
  text: Runtime Configuration
  link: /en/guide/configuration
next:
  text: Architecture Overview
  link: /en/guide/architecture
---

# Troubleshooting

This page collects the most common build and runtime issues for the current project.

## Web Console Cannot Open

Confirm that you are using host port `8080`:

```text
http://127.0.0.1:8080
```

Check container status:

- **Linux**:

  ```bash
  docker compose -f docker-compose.x86.yml ps
  ```

- **Windows (PowerShell/CMD)**:

  ```powershell
  docker compose -f docker-compose.x86.windows.yml ps
  ```

View logs:

- **Linux**:

  ```bash
  docker compose -f docker-compose.x86.yml logs -f
  ```

- **Windows (PowerShell/CMD)**:

  ```powershell
  docker compose -f docker-compose.x86.windows.yml logs -f
  ```

## Port Conflicts

The x86 Compose file publishes:

- `8080`
- `1936`
- `1985`
- `18088`
- `8000/udp`

If a port is occupied, you can modify the host port in `docker-compose.x86.yml` (or `docker-compose.x86.windows.yml` on Windows), or stop the service that occupies the port.

## No Release Package in `build_output/`

Use the full run command:

- **Linux**:

  ```bash
  docker compose -f docker-compose.x86.yml up -d --build
  ```

- **Windows (PowerShell/CMD)**:

  ```powershell
  docker compose -f docker-compose.x86.windows.yml up -d --build
  ```

For the Sophon path, use:

```bash
docker compose -f docker-compose.sophon.yml run --rm cosmo-sophon-package
```

Note: `docker compose build` only builds the image and does not necessarily execute the container command that exports the release package.

## Sophon Build Failure

The Sophon build uses a self-contained `Dockerfile.sophon` (based on `ubuntu:22.04`) and does not require an external base image.

If the build fails, check the Docker build logs:

```bash
docker compose -f docker-compose.sophon.yml run --rm cosmo-sophon-package 2>&1 | tail -50
```

Common causes:

- Network issues preventing apt/npm/cargo mirror downloads — check `SOPHON_APT_MIRROR` and related environment variables.
- Insufficient disk space — the build requires approximately 3GB.

## nginx / SRS / cosmo-engine Not Started

Run the script:

```text
${INSTALLPATH}/scripts/run_start.sh
```

The startup sequence includes:

1. Stop existing processes.
2. Start nginx.
3. Start SRS.
4. Start `cosmo-engine`.

Check the logs:

```text
/data/cwaiuserdata/log/logs
```

## Documentation Site Build Fails

First install dependencies:

```bash
npm ci
```

Then build:

```bash
npm run docs:build
```

In Windows PowerShell, if you encounter an `npm.ps1` execution-policy issue, you can use:

```powershell
npm.cmd run docs:build
```

## `vitepress` Not Found

This means the documentation-site dependencies have not been installed:

```bash
npm ci
```

## npm Audit Reports Vulnerabilities

The current documentation-site dependencies may trigger npm audit warnings. Do not blindly upgrade dependencies; before upgrading, confirm that VitePress, the theme configuration, and the GitHub Pages workflow still build successfully.

## Windows Native CPU Build

There is currently no confirmed-working Windows native CPU build script in this repository. Do not present old scripts or old commands as a publicly supported path.
