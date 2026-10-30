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
