/** Read-only snapshot interpretation. No probing, settings writes or raw diagnostic exports. */
export type DoctorState = 'reported' | 'attention' | 'unknown' | 'info';
export interface DoctorCheck {
  id: 'api' | 'version' | 'encoder' | 'capture' | 'displays' | 'sessions' | 'network' | 'compatibility';
  state: DoctorState;
  detail: string;
  action: '/logs' | '/stats' | '/settings?category=display' | '/maintenance';
  params?: Record<string, string | number>;
}
export interface DoctorReport {
  capturedAt: number;
  host: { platform: 'linux' | 'windows' | 'macos' | 'unknown'; version: string | null; commit: string | null };
  checks: DoctorCheck[];
}
function record(value: unknown): Record<string, unknown> | null {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown> : null;
}
function acknowledged(value: unknown): Record<string, unknown> | null {
  const data = record(value);
  return data?.status === true ? data : null;
}
function safeVersion(value: unknown): string | null {
  // A release identifier only. Never export an arbitrary API string, path or error.
  return typeof value === 'string' && value.length <= 100 &&
    /^v?\d+\.\d+\.\d+(?:[.+-][0-9A-Za-z.-]+)?$/.test(value) ? value : null;
}

export function buildDoctorReport(metadata: unknown, sessionStatus: unknown, capturedAt: number): DoctorReport {
  const meta = acknowledged(metadata);
  const session = acknowledged(sessionStatus);
  const rawPlatform = typeof meta?.platform === 'string' ? meta.platform.trim().toLowerCase() : '';
  const platform: DoctorReport['host']['platform'] = rawPlatform === 'linux' || rawPlatform === 'windows' || rawPlatform === 'macos'
    ? rawPlatform : 'unknown';
  const version = safeVersion(meta?.version);
  const commit = typeof meta?.commit === 'string' && /^[0-9a-f]{7,40}$/i.test(meta.commit) ? meta.commit : null;
  const encoder = record(meta?.encoder_status);
  const capture = record(meta?.capture_status);
  const display = record(meta?.virtual_display);
  const driver = record(meta?.virtual_display_driver);
  const checks: DoctorCheck[] = [
    { id: 'api', state: meta ? 'reported' : 'unknown', detail: meta ? 'api_replied' : 'api_unavailable', action: '/logs' },
    { id: 'version', state: version ? 'info' : 'unknown', detail: version ? 'running_version' : 'version_unknown',
      action: '/maintenance', ...(version ? { params: { version } } : {}) },
  ];
  if (encoder?.state === 'failed' || session?.lastEncoderProbeFailed === true) {
    checks.push({ id: 'encoder', state: 'attention', detail: 'encoder_failed', action: '/logs' });
  } else if (encoder?.state === 'ready' && encoder.h264 === true) {
    checks.push({ id: 'encoder', state: 'reported', detail: 'encoder_ready', action: '/logs' });
  } else {
    checks.push({ id: 'encoder', state: 'unknown', detail: 'encoder_unknown', action: '/logs' });
  }
  let captureCheck: DoctorCheck = { id: 'capture', state: 'unknown', detail: 'capture_unknown', action: '/settings?category=display' };
  let displaysCheck: DoctorCheck = { id: 'displays', state: 'unknown', detail: 'displays_unknown', action: '/settings?category=display' };
  if (platform === 'linux') {
    if (capture?.observed_backend === 'kms' && capture.managed_event_driven === true) {
      captureCheck = { ...captureCheck, state: 'reported', detail: 'capture_observed' };
    } else if (capture?.configured_backend === 'kms') {
      captureCheck = { ...captureCheck, state: 'info', detail: 'capture_configured' };
    }
    if (capture?.virtual_display_configured === false) {
      displaysCheck = { ...displaysCheck, state: 'info', detail: 'physical_mode' };
    } else if (capture?.virtual_display_configured === true) {
      if (display?.capable === false || display?.ready === false)
        displaysCheck = { ...displaysCheck, state: 'attention', detail: 'displays_unavailable' };
      else if (display?.capable === true && display?.ready === true)
        displaysCheck = { ...displaysCheck, state: 'reported', detail: 'displays_ready' };
    }
  } else if (platform === 'windows') {
    if (['failed', 'version_incompatible', 'watchdog_failed'].includes(String(driver?.status)))
      displaysCheck = { ...displaysCheck, state: 'attention', detail: 'driver_failed' };
    else if (driver?.status === 'ready')
      displaysCheck = { ...displaysCheck, state: 'info', detail: 'driver_ready' };
  }
  checks.push(captureCheck, displaysCheck);
  const count = session?.activeSessions;
  const knownSession = typeof count === 'number' && Number.isSafeInteger(count) && count >= 0 && count <= 10000 &&
    typeof session?.appRunning === 'boolean';
  checks.push(knownSession
    ? { id: 'sessions', state: 'info', detail: count > 0 ? 'sessions_active' : session?.appRunning ? 'sessions_paused' : 'sessions_idle',
      action: '/stats', params: { count } }
    : { id: 'sessions', state: 'unknown', detail: 'sessions_unknown', action: '/stats' });
  checks.push(
    { id: 'network', state: 'unknown', detail: 'network_unknown', action: '/stats' },
    { id: 'compatibility', state: 'unknown', detail: 'compatibility_unknown', action: '/maintenance' },
  );
  return { capturedAt: Number.isFinite(capturedAt) && capturedAt >= 0 && capturedAt <= 8.64e15 ? capturedAt : 0,
    host: { platform, version, commit }, checks };
}

export function doctorSupportSummary(report: DoctorReport): string {
  return JSON.stringify({
    schema: 'vibebeam.connection-doctor.v1',
    capturedAt: new Date(report.capturedAt).toISOString(),
    host: report.host,
    checks: report.checks.map(({ id, state, detail }) => ({ id, state, finding: detail })),
    limits: ['host-snapshot-only', 'nvenc-backend-unverified', 'client-network-unverified', 'no-package-version-check'],
  }, null, 2);
}
