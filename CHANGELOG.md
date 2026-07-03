# Changelog

All notable public changes to CosmoEdge will be documented in this file.

This project follows a release-note style inspired by Keep a Changelog.

## [Unreleased]

## [1.0.0] - 2026-07-03

First stable public release: a security and CI hardening pass plus feature, refactor, and documentation work over the `v0.1.0` baseline.

### Added

- `tools/scenario-bench` capacity/benchmark tool with pluggable CV/VLM task strategies, multi-task workloads, capacity reporting, VLM inference metrics, a 30s VLM warmup delay, and a workload design doc.
- Sophon BM1688 safety-helmet detection algorithm and models.
- x86 preset algorithm pack with ONNX models.
- Error-message i18n infrastructure for the web console.
- Gitee mirror workflow, dual-repo README, and `MIRRORING.md`.
- Default-password forced-change flow.
- Nightly Sophon build/test workflow and an x86 `cosmo-tests` CI job.

### Changed

- Hardened all GitHub Actions workflows (job timeouts, least-privilege permissions, SHA-pinned actions); pinned the Rust toolchain and frontend Node 22; rewrote `mirror-to-gitee` to direct git push with an ssh-keyscan retry.
- Renamed `PlatformConstants.h` to `NnBackendConstants.h`; aligned `DinoDetector` with `CODING_STYLE.md`.
- Build: removed redundant wrapper scripts, switched the Sophon compose flow to `run --rm`, moved Windows Sophon cross-compilation onto a Docker named volume, corrected `CMAKE_BUILD_TYPE` case, fixed vite `node_modules/.bin` permissions, switched x86 to a pre-built builder image, and excluded the test video from packages.
- Disabled the stdout log callback in production to avoid syslog flooding.
- Localized traffic-statistics dates, the onboarding channel-type hint, and task-run process name/status/algorithm fields; rendered alarm, face, and body-match similarity with consistent decimals.
- Added autocomplete and hidden username fields on auth inputs for accessibility.
- Clarified the public release roadmap in the English and Simplified Chinese README files.

### Fixed

- Hardened `util/Exec` against shell injection via an argv-based `execvp` API, migrated call sites, and added regression tests.
- Guarded `infer` `Ai*Interface` destructors against null `reuse_obj_`.
- Checked `bm_dev_request` return values in the `mem::DeviceContext` constructor.
- Guarded `nn::HungarianAlgorithm::Solve` against an empty cost matrix.
- Removed a dead null check in the `service::EventNotifier` WebSocket close callback and a dead mutex in `AlarmRecordServiceImpl`.
- Reset `SIGPIPE` to `SIG_DFL` in the child process before `execvp`.
- Fixed the auth handler test by mocking `IsDefaultPassword`.

### Security

- Pre-release hardening pass: shell-injection guards, null-safety and lifecycle fixes across `infer`/`mem`/`nn`/`service`, default-password enforcement, and child-process signal hygiene.
- Updated `SECURITY.md` with supported versions, response SLA, and hardening notes, and unified the vulnerability contact to `hello@cosmowander.ai`.

### Docs

- `SECURITY.md` supported-versions table, response SLA, and hardening notes.
- Interface HTML docs synced with the staticfile i18n versions.
- Sophon build/test CI badge and README badge layout refresh; test video added.

## [0.1.0] - 2026-06-29

Initial public open-source baseline.

### Added

- C++17 edge inference engine.
- Visual pipeline orchestrator.
- Web management console.
- x86 developer mode for Linux and Windows.
- Sophon BM1688 release packaging.
- VLM and GroundingDINO integration.
- 26 internally validated pipeline scenarios.
- Open-source README in English and Simplified Chinese.
- VitePress documentation site.
- Tutorials for quick start, scenario configuration, VLM / DINO, pipeline orchestration, and model porting.
- Build, deployment, configuration, troubleshooting, architecture, API, model/resource, frontend, and backend documentation.
- Issue templates and pull request template.
- Security policy.
- Apache License 2.0 `LICENSE`.
- Initial `NOTICE`.

### Changed

- Public quick-start command aligned with the current x86 Docker flow:
  `docker compose -f docker-compose.x86.yml up -d --build`.
- Sophon package build documentation aligned with current helper scripts:
  `scripts/build_sophon_package.sh` and `scripts/build_sophon_package.ps1`.
- README video links moved to the public `v1.0-videos` asset tag.
- Sample camera credentials and device serial values scrubbed from examples.
- Sophon build environment Dockerfile made self-contained.

[Unreleased]: https://github.com/cosmo-wander-ai/cosmo-edge/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/cosmo-wander-ai/cosmo-edge/releases/tag/v1.0.0
[0.1.0]: https://github.com/cosmo-wander-ai/cosmo-edge/releases

<!-- Gitee mirrors -->
[Unreleased (Gitee)]: https://gitee.com/cosmo-wander-ai/cosmo-edge/compare/v1.0.0...master
[1.0.0 (Gitee)]: https://gitee.com/cosmo-wander-ai/cosmo-edge/releases
[0.1.0 (Gitee)]: https://gitee.com/cosmo-wander-ai/cosmo-edge/releases

