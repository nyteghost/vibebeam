---
title: Vibebeam host log investigation — 2026-11-02
tags: [vibebeam, beta, linux, streaming, diagnostics]
---

# Host log investigation — November 2, 2026

Scope: `nyteghost/vibebeam`, `beta/host-download-20261028`. Analysis covers the supplied ZIP (local attachment named `vibepollo_logs(2).zip`), 30 logs spanning October 22–November 2, approximately 3.54 MB uncompressed. Raw logs, client identities, addresses, certificates and configuration contents are not committed. This note is Markdown suitable for Obsidian.

## Findings and source changes

| Priority | Evidence and classification | Change and remaining verification |
| --- | --- | --- |
| GPU transfer | Three GL import rejections, one November 2. EGL image creation succeeds; `glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, ...)` returns GL_INVALID_OPERATION (`0x502`) for ARGB8888 (`0x34325241`), explicit linear modifier zero. This establishes a GL binding compatibility failure, not its driver-level cause. | Preserve import then proven-linear CPU upload. Add converter-local path/layout/GL renderer diagnostics, bounded EGL modifier/external-only queries and measured CPU submission mean/max over 300 imports. Full GPU timing, external-texture compatibility and alternate GPU transfer remain hardware work. |
| Physical display baseline | Three null exceptions, including November 2 at 12:58:48. `device.value("info", object())` returns a present null; a second `.value()` then throws. The pinned libdisplaydevice schema explicitly uses optional `Info` for active displays only. | Parse the observation before publishing. Null info means inactive, no primary. Invalid IDs/types/duplicates or an empty observation preserve the previous baseline. Null/empty names fall back to a stable identity. No saved configuration is written. |
| Wayland context | Eight missing-environment errors, one November 2. KMS enumeration calls the optional `wl::init()` correlation probe from a machine service with no local user socket. | Skip only this optional KMS probe when WAYLAND_DISPLAY is absent/empty. Actual Wayland capture failures remain errors. Keep the session controller/broker, generation checks, root peer checks and user-session authority unchanged. Other real session failures remain visible. |
| KMS cursor | 64 warnings, seven November 2. No eligible hardware plane is found; this alone cannot establish whether KWin already composites its cursor. | Existing hardware-plane blending stays. Portal capture already requests embedded cursor, but it is not silently substituted for KMS on a managed NVIDIA display. No new unverified cursor image/position or root Wayland connection is introduced. Broker-supported cursor metadata fallback is still open. |
| Temporary write | Eight failures, one November 2. Prelogin loads `/usr/share/vibepollo/prelogin/apps.json`, which lacks schema version 2, then attempts to persist its migration alongside this immutable file. | Ship the already typed, UUID-bearing passive Desktop catalog at schema 2. Keep `/usr/share` immutable. Atomic-write failures now identify target, temporary path, failing operation and OS error; contents are never logged. Preserve original data and atomic replacement. |
| Audio broker | One refused connection on October 26 at 12:26:56; one response reset October 29 at 08:38:22. Both follow session-history shutdown while restoring the original physical sink. Neither repeats in November 2 logs. | Retry only audio connection establishment for ECONNREFUSED/ENOENT, at most four attempts with three 100 ms delays. No request replay after a response reset, authorization failure or generation mismatch. Current units already order broker connections before the host (reverse stop order) and controller cleanup stops the host first. Confirm deployed ordering and original-sink restoration on hardware; the historical logs do not prove current code reproduces this race. |
| Encoder probing | 201 unsupported-format reports, 24 November 2; AV1 is attempted three times per probe. RTX 3080 AV1 decode does not imply AV1 encode. An available Vulkan encoder is not the stream codec. | Existing NVENC GUID capability check remains authoritative. Briefly reuse negative codec/HDR results from an otherwise successful, resolved-adapter-owned probe (five minutes); configuration/adapter mismatch, unresolved identities, process restart, expiry and complete failures force reprobe. Do not advertise missing AV1. Three retries within a fresh probe remain; classifying authoritative native failure across legacy/native backends is follow-up work. PyroWave log explicitly describes availability only. |
| Input 0xD | 43 warnings, four November 2: one per actual streaming session. Pinned Moonlight Input.h defines ENABLE_HAPTICS_MAGIC and fixed NV_HAPTICS_PACKET. | Recognize exactly that packet size; handle as a legacy capability announcement without OS input injection. The live input handler now calls the same portable validator exercised by regression tests, replacing its duplicate parser. Retain controller permission checks, length/endian validation and rejection of every other unknown magic. This does not add new rumble capabilities or establish physical controller testing. |
| Network/logging | UPnP IPv4 discovery repeats about every 128 seconds with no router discovered. An earlier November 2 log (06:10–07:28) has five TLS warnings; the 08:49 main log has none. | Report the first discovery failure, changed error, repeat summaries every 15 minutes, and recovery; discovery retries and mapping errors remain active. LAN use does not require UPnP. Keep TLS warning visibility, verification and authentication unchanged: available evidence cannot distinguish an untrusted client, probe or genuine TLS incompatibility. |
| Proton limiter | Warnings on November 2 at 12:07:10 and 12:28:32 occur for a non-managed application while an automatic/virtual-output Proton policy is selected. Existing Steam handoff is guarded, but overlay environment is prepared earlier. | Filter Proton policy before launch environment and handoff setup when the target lacks a managed Steam identity. Native MangoHUD policy remains available. Confirm managed Proton title limits and Desktop cleanliness. |
| Picture quality | All four November 2 actual sessions show Client Requested=20000 and Host Streaming=20000 kbps. Sessions create `hevc_nvenc`. Encoder probing happens separately. | Client-selected 20 Mbps reaches the host unchanged at its initial cap stage. Logs cannot establish which client UI preset chose it. Add negotiated codec, dimensions, capture/encoding rate, original client budget, host cap, FEC and adjusted encoder target. Existing encoder-creation log identifies the implementation actually created. No client bitrate is overridden by these changes. |

## GPU compatibility work still required

`src/platform/linux/cuda.cpp` resolves the render node from the CUDA device PCI identity; do not substitute the virtual DRM card as the encoder device. The exporter's card and the GL/CUDA render device can differ legitimately. Existing EGL descriptors retain per-plane FD slots, offsets, strides and modifiers. The pinned virtual-display driver (`cd7f0eed`) returns the original DMA-BUF for an imported GEM object and uses PRIME export for native shmem objects. Therefore, the virtual driver name alone cannot identify the actual allocator. It marks implicit imported layouts unknown rather than falsely linear. The logs list both AMD and NVIDIA DRM devices as well as virtual cards; that is not evidence that encoding selected the AMD GPU. Loaded driver/source identity and actual exported-buffer ownership still need verification. Failed import does not authorize treating unknown/tiled buffers as linear; the CPU eligibility policy remains unchanged.

Khronos documents that an EGL DMA-BUF format/modifier may be external-only. Such a layout cannot simply be rebound to GL_TEXTURE_2D. The current converter samples ordinary 2D textures and shares its output textures with CUDA. A credible GPU alternative needs format/modifier query results, actual exporter/renderer identities, a validated external-texture shader/blit or compatible allocation, synchronization, color/HDR checks and latency measurements. Omitting a real modifier to silence the error is unsafe. No zero-copy fix or latency improvement is claimed.

The new EGL query is bounded to 4096 modifier entries, runs only when the converter selects a new transfer path and never changes the selected layout. A reported external-only layout is a diagnostic lead, not an implemented external-texture transfer path.

The new timing is **CPU wall time for import/fallback submission**, not GPU completion or controller-to-screen latency. It includes failed import work and CPU upload when used; it excludes color conversion, NVENC, transport, client decode and display scanout. It adds no glFinish or forced GPU synchronization. Samples cover refreshed source imports, not reused descriptors or dummy frames. Compare logs at equivalent settings, not probe-only startup frames.

## Local validation and release gates

- Focused portable GoogleTest components: 54 tests passed; the subsequently expanded input suite also passes all seven tests, including every fixed-size packet boundary. (baseline parsing, packet validation, atomic files, probe ownership/expiry, limiter policies and repeat reporting).
- Existing remote-display topology suite: 77 tests passed.
- Static machine-service/package contracts passed, including immutable prelogin schema guard. This is source/deployment contract validation, not a live session test.
- Updater/recovery suite: 21 tests passed. Existing rollback refusal remains intact.
- Existing production audio lifecycle/ownership/transition test executable passed with fake endpoints and process boundaries. This does not reproduce a real PipeWire or broker shutdown.
- Native broker-client executable passed, including connection retry limits/error classification, strict framing, capability discard and termination handling. Its intentional reset test emits an error message before passing.
- Hosted Arch build/CTest is still required before calling a package tested. Exact run/artifact outcome is reported separately after Actions completes.
- No package installation, service restart, DKMS modification, live display change or production branch update is part of this investigation.

## Safe hardware test procedure

1. Before any optional install, record the installed package version and running service unit/commit. Have a rollback **package matching that installed version**, and a fresh private configuration backup. The existing manual state backup is not a substitute for a matching package. The updater must continue to stop when this prerequisite is missing.
2. Download only the package from the exact successful beta validation run and verify its manifest/SHA256/package identity. A failure-diagnostics ZIP is not a host package. Review installation and rollback steps in [the CachyOS beta guide](BETA_CACHYOS_TESTING.md). Installation restarts streaming and is a separate, explicit user action.
3. Record physical screen positions, resolution, scale, refresh and original default audio sink. Test one client first: connect, create/select its virtual output, disconnect normally, reconnect, then close the client abruptly. Physical outputs and the default audio sink must return correctly; repeat with two clients and a user-session transition. Do not manually edit saved display JSON during this test.
4. Capture with H.264 then HEVC SDR at identical resolution/rate/bitrate. Verify path, renderer/device identity, offsets/pitches/modifier and the new CPU submission samples. Check frame pacing, drops and CPU/GPU utilization. Verify cursor across output edges, scaled output, hidden/shown state and relative/absolute input. Test HDR separately after SDR succeeds.
5. Keep requested settings fixed while comparing Moonlight/Vibelight on the same client. Controller-to-screen measurements require a repeatable external method (for example high-speed camera), not the CPU submission counter. Record display Game Mode, Shield mode, decoder, HDR and network settings. No performance claim without measurements.
6. Test audio startup, disconnect, host stop/restart and logout/login; the original physical sink must be restored without selecting a new user's sink. If a reset remains, collect broker/controller journals alongside host logs (matching timestamps/generation); host logs alone cannot prove the server-side cancellation reason. Do not relax broker authorization to recover it.
7. Verify no repeat 0xD warnings with Windows Vibelight and the Shield client; malformed packets must still be rejected. Controller buttons, triggers, mouse and keyboard must retain expected behavior. Physical rumble requires its own check.

## Wired HEVC quality experiments (client-controlled)

Starting points to **test**, not measured recommendations for this host: 1440p60 at 30–50 Mbps; 4K60 at 50 Mbps, then 60–80 Mbps if frame pacing, decode latency and loss remain stable. Increase only in the client or an explicitly selected profile. Leave host max-bitrate policy under user control. Bandwidth alone does not remove CPU fallback cost or decoder bottlenecks. The Shield target is HEVC/H.264; do not select AV1 or claim PyroWave client support based on the host probe.

Read `Negotiated stream` alongside `Creating encoder`: the former describes negotiated format/budgets, the latter an encoder instance, which can also appear during probing. Neither target bitrate nor an available-encoder log is a measurement of actual network throughput. Existing client overlays/counters supply FPS/drops/network/decode telemetry; no values are fabricated.

## References

- Pinned Moonlight protocol: https://github.com/Nonary/moonlight-common-c/blob/4ea115e8d23d1ca055a2aa8ddcfb601b705b3323/src/Input.h
- Pinned display schema: https://github.com/Nonary/libdisplaydevice/blob/18bdadbac2b21f35fe166aa1177db5c4e75829ae/src/common/include/display_device/types.h
- EGL modifier semantics: https://registry.khronos.org/EGL/extensions/EXT/EGL_EXT_image_dma_buf_import_modifiers.txt
- NVIDIA RTX 30-series encode/decode distinction: https://www.nvidia.com/en-us/geforce/news/rtx-30-series-community-qa/

## November 3 follow-up: repeated KDE helper locale warnings

The owner's installed beta `76f0d22c` ran both host/controller services, but
`kscreen-doctor` reported `Detected locale "C"` about every five seconds.
Journal cgroup metadata tied the helper to
`vibepollo-session-controller.service`; the session controller and KDE user
manager already had `LANG=en_US.UTF-8`. The five-second observations are
consistent with the controller's readiness reconciliation, not evidence of a
global locale problem.

The controller deliberately keeps `LC_ALL=C` to make privileged machine
parsing stable. Its `probe_wayland` KScreen invocation clears the inherited
environment using `/usr/bin/env -i`, unintentionally dropping `LANG`.
This beta fix supplies `LANG=C.UTF-8` **only** to the bounded KScreen child.
No change is made to controller authority, credential/socket checks, timeout,
output-size limits, JSON parsing, display selection or update behavior.

`test_session_controller.sh` now exercises the actual probe invocation with
a mocked command boundary and runs its constructed clean environment to assert
`LANG=C.UTF-8` and no inherited `LC_ALL`. The test never launches a real
KScreen process, changes monitors or restarts services.

Hardware verification is still required after a separately approved, verified
beta package is installed with a matching rollback archive and state backup:
confirm the warning count stops increasing during normal idle operation and
check video, cursor, physical output layout, audio and disconnect restoration.
Do not infer that fewer log warnings proves the full display lifecycle works.

## November 4: preflight two-factor verification on host selection

On paired HTTPS Vibebeam hosts, `GET /actions/two-factor` returns the existing
2FA status (200 if no challenge, 470 if required, 472 locked, 473 unavailable)
as the same XML status envelope used by launch/resume. After a prompt,
`POST /actions/two-factor` accepts a bounded code as the request body;
no code is placed in the URL or logged. Both operations require a verified
paired TLS identity with View permission and use `two_factor::check` with the
same per-client skip permission, replay/backup-code rules, rate limits, and
120-second grace window as the original launch/resume gate.

The `2fa-preflight` feature is only advertised by hosts with this endpoint.
Older hosts and clients continue their original launch-time 2FA flow. The
launch/resume 2FA check remains authoritative, including when a user waits in
the app grid longer than the grace window. No host policy is relaxed.

Hardware verification: install a validated matching host beta and Windows
client beta, then test initial required-code prompt at Computers, correct and
wrong codes, cancellation, timeouts, backup codes, exempt clients, and a stream
launched after the grace expires.
