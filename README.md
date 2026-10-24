# core-ci

A proposed single-tasking [SNode.C](https://github.com/SNodeC/snode.c) application for durable package-CI orchestration. GitHub Actions captures sources, builds packages and performs trusted signing/publication. core-ci owns scheduling and recovery.

**Status: initial architecture for review. No application implementation or operational deployment exists in this repository yet.**

Development is on **jupiter**, in `/home/voc/projects/snodec/core-ci`, beside the framework checkout `/home/voc/projects/snodec/snode.c`. **VOC-Experiments** is the integration organization: `ci-coordinator`, `packages`, `demo-core`, `demo-app`, `demo-side` and `demo-join`. The source repository is hosted in SNodeC; that does not authorize changing the live SNodeC package deployment.

- [Architecture, state transitions and open design decisions](docs/architecture.md)
- [Complete functional requirements and verification map](docs/requirements.md)
- [Current implementation and framework trace](docs/current-system-trace.md)
- [GitHub protocols, trust and publication boundary](docs/protocol.md)
- [Verification, API budget and implementation projection](docs/verification-and-cost.md)
- [VOC-Experiments migration plan](docs/migration.md)

The design preserves exact-tag captures, per-row progress, version coexistence, diamond gates, separate attempt budgets, package validation, signing isolation, idempotent receipts and highest-only presentation. It replaces repeated organization-wide polling and GitHub-scheduled controller wakes. One serialized publisher can run alongside concurrent builds.

Design approval is required before application implementation. Server installation, public ingress, credential/provisioning changes, sandbox cutover and any production deployment are separate stages. No builds, tags or workflow dispatches are triggered by this documentation repository.
