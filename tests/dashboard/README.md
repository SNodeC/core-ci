# Dashboard pixel checks

G5 compares both the captured-assets control and the current dashboard with the
28 original reference PNGs. Acceptance is zero differing pixels. Only the
existing attempt-id value mask applies to the current dashboard; the control
has no mask. Never regenerate references from the current render.

The reference environment uses Chrome 156.0.8078.79 on Linux, Noto Sans and
DejaVu Sans Mono system fonts, en-US, UTC, device scale factor 1, and each
reference's recorded light/dark scheme and viewport. The harness waits for the
ready model, document fonts, and two animation frames. Assets and fixture SSE
are served from a disposable 127.0.0.1 server; no external page requests are
needed. Forced colors remain off.

G5 sets FONTCONFIG_FILE to fontconfig.conf for its child browser process. This
keeps system font discovery and pins the reference's RGB subpixel order, rather
than inheriting a desktop user's fontconfig settings. An isolated XDG_CONFIG_HOME
otherwise hides those settings and changes text rasterization even with the
same font families, font files and Chrome version. It does not alter the user's
configuration or the environment of the application under test.
