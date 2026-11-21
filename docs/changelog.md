# Changelog

## Passive streaming check companion (beta)

- Add paired-client, read-only capture diagnostics with view and own-session authorization.
- Publish bounded in-memory managed transfer observations; mark inactive/stale data unavailable.
- Exclude raw logs and identifying fields; manual audio/cursor/restoration checks remain pending.
- See [companion streaming check](STREAMING_CHECK.md) for usage and limitations.

## Vibebeam beta — November 2 log reliability work

- Validate nullable physical display observations atomically; retain previous baseline on malformed data.
- Ship immutable prelogin apps at the current schema and report precise atomic-write failures.
- Recognize the fixed-size legacy haptics announcement, retaining strict input validation.
- Bound audio broker connection retries before request transmission; never replay uncertain mutations.
- Skip optional local Wayland correlation without a socket; restrict Proton policy to managed Steam targets.
- Reuse adapter-owned negative codec observations briefly; distinguish encoder availability and negotiated codec.
- Add capture transfer CPU submission timing and stream budget diagnostics; summarize repeated UPnP discovery failures.
- Preserve CPU fallback, TLS/authentication, rollback gates and production isolation.

See [investigation, limits and test procedure](VIBEBEAM_LOG_REVIEW_2026-11-02.md). Zero-copy and cursor fallback are not claimed complete.

@htmlonly
<script type="module" src="https://md-block.verou.me/md-block.js"></script>
<md-block
  hmin="2"
  src="https://raw.githubusercontent.com/LizardByte/Sunshine/changelog/CHANGELOG.md">
</md-block>
@endhtmlonly

<div class="section_buttons">

| Previous                              |                          Next |
|:--------------------------------------|------------------------------:|
| [Getting Started](getting_started.md) | [Docker](../DOCKER_README.md) |

</div>

<details style="display: none;">
  <summary></summary>
  [TOC]
</details>
