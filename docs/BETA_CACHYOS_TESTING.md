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
