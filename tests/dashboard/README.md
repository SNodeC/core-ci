# Dashboard pixel checks

G5 compares both the captured-assets control and the current dashboard with the
28 original reference PNGs. Pixel counts are advisory, as decided by the owner
after information completeness was accepted. Only the
existing attempt-id value mask applies to the current dashboard; the control
has no mask. Never regenerate references from the current render.

The reference environment uses Chrome 156.0.8078.79 on Linux, Noto Sans and
DejaVu Sans Mono system fonts, en-US, UTC, device scale factor 1, and each
reference's recorded light/dark scheme and viewport. The harness waits for the
ready model, document fonts, and two animation frames. Assets and fixture SSE
are served from a disposable 127.0.0.1 server; no external page requests are
needed. Forced colors remain off.

An isolated XDG_CONFIG_HOME can hide the capture desktop's fontconfig RGB
subpixel setting and change text rasterization with the same font families,
font files and Chrome version. Counts and diff images still report this
difference, without changing the references or adding tolerance or masks.

Capture/browser errors remain failures. G6 and the dashboard HTTP/end-to-end
tests verify information and behavior; exact historical pixels are not a gate.
