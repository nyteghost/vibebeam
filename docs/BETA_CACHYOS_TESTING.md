# CachyOS beta testing and recovery

Keep development on `beta/host-download-20261028`. A passing hosted build
validates that source and its tests, not a live KDE Wayland/NVENC session.
Do not promote to main until the runtime checks below have passed.

## Establish what is installed

Run these read-only commands in a regular Fish terminal:

```fish
pacman -Q vibebeam
uname -r
dkms status
systemctl --no-pager status vibepollo-session-controller.service vibepollo.service
```

The package version determines installed code. A downloaded archive does not.
A reachable web UI may still be served by an older running process until a
planned restart. Record both the installed version and the UI-reported runtime
version before testing. Do not infer a running upgrade from package output.

The tested rollback baseline for this beta series is **2.0.1-7**. Keep its
verified package and a configuration/state backup outside the beta download
folder. Before any future upgrade, verify the backup is readable and inspect
the rollback package using `pacman -Qip /absolute/path/to/package.pkg.tar.zst`.
Keep backups private: they can include certificates and pairing state.

Do not blindly extract a state backup over a running host. Rollback needs a
planned disconnect, known package ownership, stopped affected services, exact
archive layout/ownership inspection and restoration, then runtime verification.
Do not reboot or unload display modules merely to diagnose a tray problem.

## New interface and tray

Open `https://127.0.0.1:47990/v2/` for the new dashboard. `/settings` is the
classic UI. Its banner also links to the new interface.

The machine service intentionally disables its embedded tray. KDE uses a
separate **Vibebeam Tray** companion (older beta builds display **Vibepollo Tray**):

```fish
/usr/bin/vibepollo-tray
```

Run it as your ordinary desktop user without sudo. Installing the autostart
entry during an existing login does not itself launch it. It is packaged at
`/etc/xdg/autostart/vibepollo-tray.desktop` and is also available in the app menu.
Check KDE's hidden-icons list. Launching the helper does not restart streaming.
If no icon appears, capture its terminal error and check these read-only probes:

```fish
command -v vibepollo-tray
pacman -Q pyside6
pgrep -af '[v]ibepollo-tray'
```

Do not start a second streaming host to obtain a tray icon. The companion uses
a singleton lock. Newer beta builds open `/v2/` directly from its Open action. Its API connection is separate from host pairing; never paste
API tokens, private keys, state archives or full configuration into test reports.

## Connection Doctor

New beta builds include `https://127.0.0.1:47990/v2/doctor`. Run checks from the
new dashboard or navigation. It reads host metadata and session state without
starting capture or encoder probes, changing settings or stopping services.

Check that configured KMS capture is distinguished from observed capture,
unknown network/client compatibility is visible, and failed refreshes remove
older positive findings. The version shown is the running host version, not a
package database check. Use a local terminal to verify the installed package.

Preview the support summary before explicitly copying it. It contains static
check outcomes and supported running-version identifiers, not raw logs, tokens,
paths, addresses or client/app names. Nothing is uploaded by this feature.

## Controlled smoke test

Use local physical access and a short test session, with the rollback package
and backup available. Record pass/fail and exact client/host commit versions.

- Verify physical monitors and the existing desktop before connecting.
- Pair/connect with the existing client; verify video, audio, keyboard and mouse.
- Check NVENC availability and normal quality on the RTX 3080.
- Open the new dashboard at desktop and narrow widths; keyboard focus remains visible.
- Switch Smart Local to Off, reload, and verify Off persists in this browser.
  Local AI remains unavailable. Newer beta builds measure host-output FPS per
  selected active session after a continuous observation window. Client FPS,
  latency and packet loss remain unknown. Apply/Undo is unavailable. Switch Off
  or hide the page to stop sampling; verify reconnects start a fresh window.
- Test a brief client-side network interruption, recovery and explicit disconnect.
  Do not power-cycle the production host as an outage simulation.
- Verify virtual displays owned by the session disappear and physical outputs remain.
- Verify the host can accept a new session and the local desktop remains usable.
- Check top-center and bottom-right bitrate notifications on the client.

Stop testing on loss of physical output, capture errors, pairing loss, failed
cleanup or unexpected settings changes. Preserve redacted logs and use the
prepared rollback procedure rather than repeated installs or forced restarts.

## Validation record, October 31, 2026

At the start of this continuation the exact host head `44ad2139e3f8` passed
[Arch validation](https://github.com/nyteghost/vibebeam/actions/runs/41212066022).
This is a baseline, not certification of subsequent commits or CachyOS runtime.
The UI unit suite and production web build must also pass on each changed head;
the dedicated Beta web validation workflow covers these plus dashboard browser
checks. Hosted tests never install onto the production streaming machine.

## Single-command manual beta updates (Arch/CachyOS x86_64)

New beta packages include an ordinary-user terminal command, compatible with Fish:

```fish
vibebeam-update --check
vibebeam-update
```

`--check` only reads public release metadata and local build identity. It does
not request sudo, install packages, or change services. Updates are manual;
there is no timer, background polling, or automatic reboot.

The updater selects only `host-beta-RUN-ATTEMPT` GitHub prereleases produced by
the beta publication job. It verifies the exact source commit, repository,
branch, architecture, canonical asset URL, size and SHA-256, then inspects the
Arch package name/version before invoking pacman. Release checksums are fetched
over verified HTTPS from this repository; they are not independent package signatures.
The publication job waits for the Arch package/component tests and the web
unit/build/recovery/diagnostics tests. It leaves the stable latest release unchanged.

Before installation, close every streaming client and type `UPDATE` at the
prompt. The updater does **not** have authenticated live session detection yet;
the confirmation records your explicit disconnect and restart decision. Pacman
also asks for its normal confirmation. Installing native packages executes their
normal service/DKMS hooks, and the updater restarts `vibepollo.service` after a
successful install. It does not restart the display manager, unload modules, or reboot.
An active systemd service does not prove capture or streaming is healthy.

A package matching `pacman -Q vibebeam` must be available for rollback. The
updater searches `/var/cache/pacman/pkg` and its private
`~/.cache/vibebeam-updates` cache. If your verified rollback archive lives elsewhere:

```fish
vibebeam-update --rollback /absolute/path/to/your-old-vibebeam.pkg.tar.zst
```

Before pacman runs, a sudo-controlled snapshot is created under
`/var/lib/vibepollo-package-backups/update-*/`, with private directory/file
permissions. It contains `state.tar.gz` (existing `/etc/vibepollo` and
`/var/lib/vibepollo`), `rollback.pkg.tar.zst`, and `installed.txt`. The backup
archive is listed to verify readability. Existing manual backups are retained.
Downloaded incoming packages are retained for later rollback lookup. A backup
or verification failure stops installation. There is no automatic rollback:
restoring machine state requires a planned service stop and ownership inspection.

### First update from an older package

Older installations do not contain `vibebeam-update`. On the repository's
[releases page](https://github.com/nyteghost/vibebeam/releases), choose a
**Vibebeam beta COMMIT** release with a `host-beta-` tag. Download
`vibebeam-update.py` and `SHA256SUMS` into a new directory. Check the updater:

```fish
sha256sum --check --ignore-missing SHA256SUMS
python3 ./vibebeam-update.py --check
python3 ./vibebeam-update.py --rollback /absolute/path/to/your-old-vibebeam.pkg.tar.zst
```

Inspect the checksum result before running Python. After the beta package is
installed, use `vibebeam-update` from any terminal. GitHub CLI credentials are
not needed for public release downloads. Source, build recipe, provenance,
checksums and the bootstrap updater are published alongside the package.

### Build identification

The modern dashboard's sidebar, status panel, Overview and Maintenance version
labels display `Beta · SHORTCOMMIT` for beta builds. The full commit remains in
Maintenance and the raw version remains available to update comparison logic.
Unknown commits are explicitly shown as unavailable, never guessed from a tag.
This identifies the running host; installing a package alone does not prove that
the running service has restarted into it.

### Verification boundaries

Automated tests cover download/trust limits, substituted manifests, wrong
source/package identity, rollback package matching, and generated release assets.
The hosted build checks package contents and runs web/browser tests. A real
CachyOS installation, permission prompts, rollback restoration, tray visibility,
and NVENC/Wayland streaming must still be verified on the physical host.
