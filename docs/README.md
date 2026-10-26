# Documentation guide

core-ci has two audiences: people operating a package pipeline and people changing its code.
Both need the same central idea: **the graph authorizes work; GitHub jobs execute it**.
A green GitHub build is an intermediate result. A prerequisite becomes usable only after
core-ci has verified publication to the configured HTTPS package feed.

## Suggested reading paths

**To understand the system**, read [architecture and algorithms](architecture/README.md)
from the concepts through the sequence diagrams. The diamond example explains why packages
can be individually successful while a dependent still correctly waits.

**To add your project**, read the definition and hook sections in the
[extension guide](extensions/README.md). You usually need repository JSON and a project
hook, not a controller change. Then follow enrollment in the deployment guide.

**To install a complete deployment**, follow [deployment and operations](deployment/README.md)
in order. It distinguishes public settings, server-side credentials and publication secrets.
It also covers an installation behind a home router, certificate renewal, backup and recovery.

**To change implementation behavior**, read the architecture first, then the extension
checklists. Changing a state transition or assignment format affects more than the UI or a
single Python module: the controller, pinned workflows, validators and persisted evidence
must agree.

## GitHub and packaging vocabulary

| Term | Meaning in core-ci |
| --- | --- |
| Repository | A GitHub project containing files and Git history |
| Commit SHA | The immutable identifier of a particular Git commit |
| Tag | A name, such as `v2.1.3`, pointing to a commit directly or through an annotated tag object |
| Branch/ref | A movable Git reference, such as `main`; resolved before use where immutable input is required |
| Workflow | A YAML file defining GitHub Actions jobs |
| Workflow dispatch | An authenticated REST request asking GitHub to start a configured workflow |
| Run | One GitHub execution of a workflow, with a numeric run ID |
| Job | A group of steps inside a run, executed by a runner |
| Runner | The machine executing a job; chosen by the explicit target selector |
| Reusable workflow | A workflow called by another workflow, pinned here to a full trusted commit |
| Artifact | Files retained by GitHub Actions; an intermediate build output, not a published package repository |
| Webhook | An HTTPS notification from GitHub to the controller |
| GitHub App | An application identity with explicit installation grants and short-lived tokens |
| Assignment | The controller's immutable description of the work it has authorized |
| Execution | One graph node for selected source, target and prerequisite executions |
| Attempt | A particular build/publication effort for that execution, with its own rebuild revision |
| Receipt | Evidence of an actual feed publication and the package/export identities it contains |
| Feed | An HTTPS package repository usable by apt, dnf, opkg or the matching APK client |
| Development export | A project's own headers/libraries or SDK development files needed by downstream builds |

A **source version** is the selected semantic-version tag. A **distribution suite** is the
feed series, such as Debian `trixie` or OpenWrt `25.12`. A **package rebuild revision**
distinguishes another attempt even when the source version has not changed. These values
answer different questions and are not interchangeable.

## How to read the diagrams

The sequence diagrams show messages in time order, from top to bottom. A solid arrow is a
request or operation; a dashed arrow is a response. Participants represent existing concrete
components, not proposed services or new schedulers. A database commit shown before a dispatch
is intentional: the assignment survives a lost HTTP response or a controller restart.

The source references beside each algorithm point to its implementation. The documentation
explains the reason for a boundary; the referenced source defines the exact supported fields
and checks. Examples use the sandbox names for clarity. They are deployment settings rather
than a compiled-in list of supported projects or distributions.
