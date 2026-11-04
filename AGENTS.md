# Phase 1 development

The canonical checkout is /home/voc/projects/snodec/core-ci. Follow SPEC.md.
Archive tags are never implementation references. No additional checkouts.
Persistent builds use build/codex/<configuration>; temporary checks use
/tmp/codex/build/core-ci/<task>.

The canonical formatting and analysis files and cmake modules are byte copies
from ~/.codex/formatting. The modules assume src/; this repository instead uses
explicit format and format-check targets for core/ and tests/ plus the top-level
CMakeLists.txt. The unchanged canonical modules remain committed. Discovery is
restricted to these owned source directories, excluding generated builds,
third-party files, checkouts and editor files. The analyze target uses the
committed .clang-tidy; clangd uses .clangd and the build's compile_commands.json.
