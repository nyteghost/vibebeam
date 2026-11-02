# Vibebeam beta — reliability and safe host release

Status: **in progress** (October 31, 2026). Scope: the beta branch only.
Production CachyOS / KDE Wayland must remain on the known-good installed host
until a verified package, rollback procedure, and smoke test exist.

## Completed source changes (not equivalent to validated releases)

- Failed Arch CTest logs are uploaded as a diagnostic artifact.
- Generated local package metadata uses `vibebeam` consistently.
- Release-link and locale regression fixtures were corrected.
- Host configuration docs and UI locale include `file_transfer_dir`.
- Cross-platform fixtures for native display-mutation admission were updated.
- Subprocess display-power lease test is run without competing CTest jobs;
  all its internal concurrent owner assertions remain in place.

## Required release gates

- [ ] Hosted Arch validation **passes**, including every required CTest.
- [ ] One correct `vibebeam-*.pkg.tar.zst` package, manifest, and hashes
      produced. A diagnostics artifact is **not** a deployable package.
- [ ] Exact package ownership, DKMS driver compilation, ABI/dependencies,
      services, and rollback/preflight logic verified.
- [ ] No production install or main-branch promotion before validation.
- [ ] Clean disconnect, session expiry, crash recovery, and connection-loss
      behavior verified with physical monitors preserved.
- [ ] Only virtual displays created/owned by Vibebeam are ever removed.
- [ ] Client/host protocol/version compatibility checked before updates.
- [ ] CachyOS manual smoke test and explicit, reversible install procedure.

## Next improvements (not yet implemented or validated)

- Host watchdog for abandoned sessions; bounded cleanup and clear audit log.
- NVENC and KDE/Wayland capture fault diagnostics.
- Host-side safeguards for bounded Vibelight reconnects.
- Pairing, certificate pinning, and scoped API permission audit.
- Verified host update channels and safe rollback, with explicit user choice.
- Automated end-to-end stream/reconnect/teardown/virtual-display tests.
- Redacted diagnostic export without tokens or personal file contents.

Prior failed validation:
https://github.com/nyteghost/vibebeam/actions/runs/41100992709

## October 31 implementation checkpoint

- [x] Host UI: persistent accessible status strip, refresh and stats link, responsive styling.
- [x] Read-only offline quality advisor module with sustained-trend thresholds and missing-data checks.
- [x] Advisor unit tests and a host dashboard panel with Off / Smart Local selection.
- [x] Connect host-output FPS from per-session frame/uptime deltas; isolate sessions and reset on gaps/reconnects.
- [ ] Add independently verified client RTT, packet-loss and delivered-FPS signals; unavailable metrics remain unknown.
- [x] Persist explicit mode and dismissed suggestion types per browser/host origin; recover from blocked storage.
- [ ] Implement transactional Apply / Undo with automatic rollback; **not available** yet.
- [ ] Enable optional Local AI explanations only after resource/privacy tests.
- [ ] Confirm latest beta CI, Arch validation and production-independent smoke tests.

Source commits: `a3eca0548`, `0f99f46c6`, `4461ffe9a`,
`6fc0ec20f`, `13c2506bd`, `51cdd1936`.
No production CachyOS installation authorized by this checkpoint.

## Vibe Intelligence — quality recommendations with safe apply/undo (planned)

- [ ] Collect local per-session quality signals (FPS, frame pacing, bitrate,
      RTT/jitter, packet loss, encoder/capture latency and drops where available),
      with explicit missing-data states and no telemetry upload by default.
- [ ] Recommend a specific, supported quality adjustment only when a sustained
      trend and minimum sample window justify it; show the observed evidence,
      predicted trade-off, and confidence/uncertainty. Avoid repeated prompts
      with hysteresis and cooldowns.
- [ ] Present **Apply**, **Not now**, and **Don't suggest again for this host**;
      never silently change resolution, refresh rate, codec, or virtual displays.
- [ ] On Apply, snapshot all affected client and host settings, verify capability
      and compatibility, change only the scoped parameters, and confirm the
      stream remains healthy within a bounded observation window.
- [ ] Provide a prominent **Undo** that restores the exact prior settings;
      auto-rollback on failure, disconnect, timeout, or degraded stream, with
      explicit feedback if any setting cannot be restored.
- [ ] Persist only user-approved per-host preferences, make suggestions
      opt-out, and avoid GPU-intensive inference during active streaming.
- [ ] Test unsupported settings, stale telemetry, rapid reconnects, competing
      manual edits, failed apply, partial rollback, and host/client version
      mismatch before enabling in beta.

Status: **partially implemented, read-only**. Offline rules and browser preferences exist;
live telemetry, transactional Apply/Undo and optional AI explanations remain unimplemented.

## Vibe Intelligence — operating modes (planned)

- [ ] **Off:** Disable Vibe Intelligence recommendations and AI explanations;
      core streaming, manual quality controls, and essential safety protections
      continue working normally.
- [ ] **Smart Local (default):** Free, offline-first rules and lightweight
      statistical analysis for quality/connection recommendations. No model,
      account, subscription, API key, cloud service, or GPU inference required.
      All proposed setting changes require explicit Apply, support Undo, and
      automatically roll back on failed or degraded results.
- [ ] **Local AI (opt-in):** Add optional plain-language diagnostic explanations
      using a user-installed local model. Never require or automatically download
      a model. Allow inference to pause during active gaming/streaming; use
      sanitized summaries rather than raw secrets, tokens, or file contents.
- [ ] Keep all modes optional, selectable in settings, and usable without
      internet access. No telemetry or diagnostics leave the device by default.
      No cloud AI dependency in the core product; any future cloud integration
      must be a separately consented optional feature.
- [ ] Persist the chosen mode locally, clearly indicate model availability,
      and degrade gracefully from Local AI to Smart Local when a model is absent
      or resource limits are reached (without silently enabling cloud access).
- [ ] Test offline operation, mode switching, opt-out, no-model behavior,
      resource usage during streams, privacy boundaries, and Apply/Undo safety.

**Implementation status:** Off / Smart Local selector and local browser preferences are
implemented on beta. Local AI is unavailable; selecting a legacy saved AI preference
falls back to rules. No setting changes, inference or uploads are enabled.

## Continuation: evidence validation and test coverage

- Reject replayed timestamps, stale metrics, observation gaps and impossible percentages.
- Require the minimum sample span separately for each metric, not just the overall batch.
- Save only explicit browser choices; no quality samples, credentials or model data.
- Add fast hosted web unit/build and dashboard browser checks on the beta branch.
- See [CachyOS testing and tray recovery](BETA_CACHYOS_TESTING.md).
- Initial head `44ad2139e3f8` passed hosted Arch validation; each new head still needs
  its own passing package build and runtime validation before promotion.

## Next beta increment: live observations and desktop clarity

- Read existing `/api/history/sessions/active` data without changing host state.
- Sample host-output FPS every five seconds while the dashboard is visible and
  Smart Local is enabled. Off and page suspension abort pending reads and clear
  observations. No samples persist or leave the device.
- Show a session selector, measured FPS/target and evidence count. At least six
  continuous observations over 15 seconds are required for guidance. Static
  content can lower host FPS: this alone does not diagnose client/network health.
- Separate counters per session; reject duplicate IDs, replayed uptime, counter
  resets, target changes, long gaps and unsupported/malformed endpoint responses.
- Rename the visible Linux companion to Vibebeam Tray; Open leads to `/v2/`.
  Preserve `vibepollo-tray`, the existing lock, settings and service identities.
- Correct browser fixtures for this fork's release endpoint while retaining the
  network audit and upstream compatibility. Assert the unavailable AI option's
  actual disabled DOM property.

## Candidate additions, ordered by everyday value

1. **Connection doctor:** read-only checks for version compatibility, NVENC,
   capture and monitor readiness; distinguish unknown from failed and show the
   relevant next step. No automatic service restarts or module changes.
2. **Per-client profiles:** remember quality/display choices for each client;
   preview the exact changes and preserve manual overrides.
3. **Session review:** explain FPS trends, bitrate changes, reconnects and cleanup
   results using known evidence; provide a local redacted summary for support.
4. **Monitor preview and recovery:** show physical versus owned virtual outputs,
   preview changes, and provide explicit restore controls that preserve other displays.
5. **Update center:** show installed/running/available versions separately,
   verified package provenance and rollback readiness before a planned update.

These are planned additions, not release claims. Transactional Apply/Undo,
client telemetry, Local AI and runtime capture/disconnect verification remain
open release work. Implement telemetry and capability checks before enabling
any automatic quality transaction.

## Connection Doctor: implemented on beta

- New `/v2/doctor` page, linked from the dashboard and navigation.
- Read-only fresh metadata/session snapshots with an eight-second timeout.
  A refresh clears older findings; unavailable and malformed responses stay unknown.
- Separate encoder probe evidence, configured/observed Linux KMS capture,
  virtual-output/Windows-driver readiness and active/paused/idle session state.
- Distinguish the running version from the installed package. Do not claim NVENC,
  client latency/loss, compatibility, output ownership or physical-monitor cleanup
  from snapshots that do not expose them.
- Preview and explicitly copy a local support summary containing only running
  version identifiers, check outcomes and known limitations. No automatic upload,
  raw errors/logs, configuration, client/app names, addresses, tokens or file paths.
- Keyboard/mobile layout and clipboard failure coverage are in the hosted browser
  gate. The existing end-to-end stream and CachyOS smoke-test gates remain open.
- Newer Arch validation runs supersede older runs in the same branch concurrency
  group, reducing duplicate package builds during beta iteration.

## Further additions to consider after profiles

- Quick Connect favorites for frequently used host/app combinations.
- Gaming, Desktop and Battery Saver presets, with a preview and manual overrides.
- Transfer center with per-file progress, cancellation, destination visibility,
  checksums and verified resume after interruptions.
- A focused in-session toolbar for quality, screenshots, displays and disconnect.

These are future work, not implemented by the Connection Doctor increment.


## November 2 — manual beta updater and build identity

Implemented on beta, awaiting the linked hosted package and physical install checks:

- `vibebeam-update` / `--check`: credential-free public beta release selection,
  bounded HTTPS downloads, source/package/hash checks and explicit installation.
- Matching rollback package required; private state/rollback snapshot before pacman.
- Preserve incoming packages in a private cache for subsequent rollback lookup.
- Beta Arch publication waits for component/package and web/browser gates, publishes
  source/provenance/bootstrap assets as a prerelease, and preserves stable latest.
- Chronological beta package suffixes; installed source/run identity packaged.
- Modern host version labels display `Beta · SHORTCOMMIT`; raw API version preserved.
- Remaining: authenticated active-session preflight, physical CachyOS update/rollback
  and streaming verification, and independent release signing. No automatic updates.


### Button-driven channel updates

- Implemented Maintenance Main/Beta selection, verified availability and commit,
  native desktop handoff, channel persistence and fresh authenticated idle checks.
- Native URI accepts only literal channels; ordinary-user handler opens a terminal
  and retains the existing backup, rollback, package and permission checks.
- Release API body carries bounded manifest identity for CORS-safe UI checks.
- Main protocol is supported but unavailable until an approved verified production
  package exists. No promotion or production branch change was made.
- Remaining physical checks: KDE URI registration, browser prompt, terminal/password
  interaction, actual update/rollback and post-update streaming.

### Publication recovery

- Removed the superseded-head skip that could produce a green run with no release.
- Added recovery publication for the two affected updater builds, with original
  source/branch/repository and successful gate checks before reading artifacts.
- Existing public releases are preserved; releases remain prereleases/non-latest.
- Added negative admission tests for forks, failed/skipped/incomplete gates and
  substituted source identities. No host installation is performed by recovery.
