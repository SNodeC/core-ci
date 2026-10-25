# SNode.C URL encoding fix

Owner approved applying the fix and pushing without a new branch. Commit
1092fda70a60dabd7ac4cf63dc109551784482bc was pushed directly to SNodeC/snode.c master.
The canonical checkout remains on its existing fix/receiver-removal-callbacks branch;
unrelated receiver edits were preserved. The owner subsequently updated the installed framework.

The installed public httputils::url_encode("v1.0.0+build") returned v1.0.0%0+build.
RFC 3986 requires v1.0.0%2Bbuild. The stream inserts unsigned char as a character rather
than its numeric hexadecimal value. Character classification should also use ASCII
unreserved characters rather than locale-dependent isalnum on a possibly negative char.

The upstream production change affects only url_encode. Public-boundary regression cases:

| Input | Expected |
| --- | --- |
| v1.0.0+build | v1.0.0%2Bbuild |
| a/b c?&= | a%2Fb%20c%3F%26%3D |
| UTF-8 bytes C3 A4 | %C3%A4 |
| AZaz09-._~ | unchanged |

The baseline probe used find_package(snodec COMPONENTS http) and the installed snodec::http
interface under /tmp/codex/build/core-ci/url-encode-probe. The corrected framework was built in /tmp/codex/build/snode.c/url-encoding; the encoder
regression and existing HTTP query-parser test pass. The regression fails against the
installed buggy library. No installation was performed; installation is a separate action.

After the owner reported installation, the installed probe returned v1.0.0%2Bbuild.
core-ci now calls httputils::url_encode directly for tag and branch references; its local
encoder was deleted. The source-selection boundary test requests v0.9.0+build through
its encoded URL and verifies that the recorded raw tag retains its literal plus sign.
