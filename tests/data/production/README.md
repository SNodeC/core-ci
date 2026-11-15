# Production contract fixtures

Byte-for-byte copies of default-branch HEAD files captured on 2026-11-15.
Packages supplies ci/registry.json and ci/targets.json; each source supplies
ci/packages.json. The test resolves these captured HEADs as release commits
and also exercises the no-release default-branch bootstrap path.

| Repository | Branch | HEAD |
| --- | --- | --- |
| SNodeC/Packages | main | baa8f8962bb5597d333bb2fc6e702d2b1a98618b |
| SNodeC/snode.c | master | 652e9599e8809f403d717dd7c167ace7df0b7220 |
| SNodeC/mqttsuite | master | 1ae362c2d0a6a1c3753e47c7dae831bdfeac620a |
| SNodeC/snode.c-idp | main | 42f8ef282d6c91ea79cc55ff2c39f2703184ba92 |
| SNodeC/core-ci | main | e2d2da4ab9bd927d16de5bb14170b4b0ea9a3772 |
| SNodeC/AISuite | master | eaeae228e3b8ddd780edf462d2cd6f7aa9e2b5b3 |
| SNodeC/CodexUI | master | 93eff0eaa1e910e6ad9102643d69d49b54f30b21 |

Expected package×target units: snodec 40, mqttsuite 40, snodec-idp 26,
core-ci 25, aisuite 26, codexui 16; total 173 across 40 catalog targets.
