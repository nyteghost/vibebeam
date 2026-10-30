# Vibebeam beta — reliability and safe host release

Status: **in progress** (October 30, 2026). Scope: the beta branch only.
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

Status: **roadmap only; not implemented**. Initial recommendations should use
transparent local rules/statistics; optional AI explanation can follow later.

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

**Implementation status:** roadmap only; these modes are not yet shipped.
