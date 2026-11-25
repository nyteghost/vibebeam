# Non-Steam games in the Steam library

Vibebeam's Steam provider now discovers saved, non-hidden non-Steam shortcuts
alongside normal Steam installations. They appear under **Steam** in
**Library → Add application → Browse games** and the name autocomplete.
Selecting one uses the existing managed Steam application flow; clients do not
need a protocol change.

## Discovery and launch

- Reads `userdata/<account>/config/shortcuts.vdf` from the already-discovered
  Steam roots, including standard/Flatpak roots. Steam must have saved the
  shortcut to disk before a refresh can find it. Nothing writes to Steam files.
- Uses Steam's persisted unsigned 32-bit `appid`, name and last-played value.
  Renaming with the same saved ID updates the existing managed entry. Different
  IDs are distinct even when titles match; repeated IDs are deduplicated.
- Recognizes local portrait and landscape artwork from that account's
  `config/grid` directory. Existing artwork conversion remains unchanged.
- Shortcuts count as registered/available for selection, **not proof the target
  executable exists**. Steam resolves the target and applies its saved launch
  options, Proton selection and Steam Input. Recent-only sync still requires a
  last-played timestamp; use Browse games for a shortcut you have not played.
- Keeps the existing numeric catalog/API identity, converting only the launch
  URI to Steam's 64-bit shortcut GameID: `(appid << 32) | 0x02000000`.
  Normal Steam titles retain their existing launch path.
- If multiple Steam accounts have saved shortcuts, discovery sees the readable
  profiles of the selected OS user. Sign into the Steam account that owns the
  shortcut before launching. Duplicate IDs use the first sorted account within
  a root and the existing root priority; artwork is not mixed between accounts.

## Safety and limits

On the Linux machine host, the existing dropped-UID scanner parses these files.
Only bounded catalog metadata crosses back to the host. `Exe`, `StartDir`, icon
paths stored inside the shortcut and `LaunchOptions` are never exported or
executed by the host. The authenticated broker still accepts only a bounded
numeric ID and constructs the fixed Steam URI itself after entering the desktop
session. It does not accept arbitrary URIs or command text.

The binary reader limits each file to 16 MiB, 4,096 entries, eight nested levels,
131,072 fields and bounded UTF-8 display names. Unknown binary types, duplicate
keys, truncated data and trailing garbage reject that file without discarding
normal Steam games or other readable shortcut files. Empty/unsafe names, invalid
IDs and hidden shortcuts are skipped. IDs are not guessed from names or paths.

Direct-launch environment injection is unavailable for shortcuts; configure
their launch settings in Steam. Existing session-wide host policies are unchanged.
No install directory or process ownership is inferred from a shortcut's arbitrary
working directory. Untrackable launches retain detached streaming behavior:
disconnecting does not promise to close the game; close it inside Steam/the game.

## Verification

Automated fixtures cover binary parsing and bounds, unsigned IDs, Unicode,
hidden entries, malformed/truncated files, duplicate roots/accounts, artwork,
renaming, recent/excluded selection, idempotent catalog reconciliation, safe
scanner round-trip, Steam URI generation and the broker's exact exec arguments.
The focused workflow compiles the actual provider on Linux and Windows and runs
the Linux broker policy tests. This does not prove an actual Steam launch.

Physical CachyOS/Steam validation remains required:

1. Use a separately validated host build only after preserving a rollback
   package matching the installed version and a private state backup. Follow
   the existing [CachyOS testing guide](BETA_CACHYOS_TESTING.md); this document
   does not authorize an installation or a service restart.
2. Refresh Browse games after Steam saves a shortcut. Check one Zelda entry,
   a normal Steam title, the displayed name and local artwork.
3. Add the shortcut and launch it from Vibelight or stock Moonlight. Verify its
   existing Steam launch options/compatibility selection and input behavior.
4. Disconnect/reconnect; check streaming and display/audio restoration. Close
   the shortcut's game manually if its lifecycle is not trackable.
5. Record exact host/client commits and pass/fail. Do not include credentials,
   private state archives or full shortcut files in the report.

Format references: Valve's [CGameID layout](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/steam/steamclientpublic.h)
and [persisted shortcut ID discussion](https://github.com/ValveSoftware/steam-for-linux/issues/9463).
