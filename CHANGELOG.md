# Changelog

All notable public changes to CosmoEdge will be documented in this file.

This project follows a release-note style inspired by Keep a Changelog. CosmoEdge is currently versioned as `v0.1.0` while the project prepares the `v1.0` stable release.

## [Unreleased]

### Changed

- Clarified the public release roadmap in English and Simplified Chinese README files.
- Unified the security vulnerability contact email as `hello@cosmowander.ai`.

### Planned

- Complete the final `v1.0` regression pass.
- Publish the `v1.0` release tag and release notes.
- Expand the validated pipeline scenario library.
- Add community model examples, scenario examples, model adapters, and post-processing templates.

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

[Unreleased]: https://github.com/cosmo-wander-ai/cosmo-edge/compare/main...HEAD
[0.1.0]: https://github.com/cosmo-wander-ai/cosmo-edge/releases
