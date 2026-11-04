import assert from 'node:assert/strict';
import test from 'node:test';
import { buildDoctorReport, doctorSupportSummary } from '../utils/connectionDoctor.ts';

const idle = { status: true, activeSessions: 0, appRunning: false, paused: false, lastEncoderProbeFailed: false };
const metadata = {
  status: true, platform: 'linux', version: '2.0.1-beta.1', commit: 'a'.repeat(40),
  encoder_status: { state: 'ready', h264: true },
  capture_status: { configured_backend: 'kms', observed_backend: 'unknown', managed_event_driven: false, virtual_display_configured: true },
  virtual_display: { capable: true, ready: true },
};
const check = (meta: unknown, session: unknown, id: string) => buildDoctorReport(meta, session, 1000).checks.find((row) => row.id === id)!;

test('configuration is distinguished from observed KMS capture', () => {
  assert.equal(check(metadata, idle, 'capture').detail, 'capture_configured');
  const observed = { ...metadata, capture_status: { ...metadata.capture_status, observed_backend: 'kms', managed_event_driven: true } };
  assert.equal(check(observed, idle, 'capture').detail, 'capture_observed');
  assert.equal(check({ ...observed, capture_status: { ...observed.capture_status, managed_event_driven: 'true' } }, idle, 'capture').detail, 'capture_configured');
});
test('encoder success needs explicit H264 capability; failure remains visible during a stream', () => {
  assert.equal(check(metadata, idle, 'encoder').state, 'reported');
  assert.equal(check({ ...metadata, encoder_status: { state: 'ready', h264: 'true' } }, idle, 'encoder').state, 'unknown');
  assert.equal(check(metadata, { ...idle, activeSessions: 1, appRunning: true, lastEncoderProbeFailed: true }, 'encoder').state, 'attention');
  assert.equal(check({ ...metadata, encoder_status: { state: 'failed', h264: true } }, idle, 'encoder').state, 'attention');
});
test('physical-display mode does not require virtual-output support', () => {
  const meta = { ...metadata, capture_status: { virtual_display_configured: false }, virtual_display: { capable: false, ready: false } };
  assert.equal(check(meta, idle, 'displays').detail, 'physical_mode');
  assert.equal(check(meta, idle, 'displays').state, 'info');
  assert.equal(check({ ...meta, capture_status: { virtual_display_configured: true } }, idle, 'displays').state, 'attention');
});
test('missing or malformed responses stay unknown instead of being interpreted as ready or idle', () => {
  for (const invalid of [null, false, [], {}, { status: false }, 'offline']) {
    const report = buildDoctorReport(invalid, invalid, 1000);
    assert.ok(report.checks.every((row) => row.state === 'unknown'));
  }
  for (const value of [-1, 0.5, Infinity, NaN, '0', 10001])
    assert.equal(check(metadata, { ...idle, activeSessions: value }, 'sessions').state, 'unknown');
});
test('session state distinguishes active, paused and idle without claiming display cleanup', () => {
  assert.equal(check(metadata, idle, 'sessions').detail, 'sessions_idle');
  assert.equal(check(metadata, { ...idle, appRunning: true }, 'sessions').detail, 'sessions_paused');
  assert.equal(check(metadata, { ...idle, appRunning: true, activeSessions: 2 }, 'sessions').detail, 'sessions_active');
});
test('Windows driver metadata does not prove capture or Linux virtual readiness', () => {
  const meta = { ...metadata, platform: 'windows', virtual_display_driver: { status: 'ready' } };
  assert.equal(check(meta, idle, 'capture').state, 'unknown');
  assert.equal(check(meta, idle, 'displays').detail, 'driver_ready');
  assert.equal(check({ ...meta, virtual_display_driver: { status: 'version_incompatible' } }, idle, 'displays').state, 'attention');
});
test('network and client compatibility remain unknown even when streaming', () => {
  for (const id of ['network', 'compatibility'])
    assert.equal(check(metadata, { ...idle, activeSessions: 1 }, id).state, 'unknown');
});
test('support summary allowlists version and findings, omitting raw metadata and personal data', () => {
  const meta = { ...metadata, hostname: 'PRIVATE-HOST', token: 'SECRET-TOKEN', ip: '10.200.1.149', path: '/home/private', branch: 'private-branch',
    virtual_display: { ...metadata.virtual_display, reason: 'PRIVATE-REASON' } };
  const session = { ...idle, appName: 'PRIVATE-APP', client_name: 'PRIVATE-CLIENT' };
  const before = structuredClone({ meta, session });
  const report = buildDoctorReport(meta, session, 1000);
  const text = doctorSupportSummary(report);
  assert.equal(JSON.parse(text).host.version, metadata.version);
  assert.ok(!/PRIVATE|SECRET|10\.200|\/home/.test(text));
  assert.deepEqual({ meta, session }, before);
  assert.deepEqual(JSON.parse(text).limits, ['host-snapshot-only', 'nvenc-backend-unverified', 'client-network-unverified', 'no-package-version-check']);
});
test('untrusted version, platform and commit strings never enter a copied summary', () => {
  const report = buildDoctorReport({ ...metadata, platform: 'PRIVATE-HOST', version: '/home/PRIVATE-FILE', commit: 'SECRET-TOKEN' }, idle, NaN);
  assert.deepEqual(report.host, { platform: 'unknown', version: null, commit: null });
  assert.equal(report.capturedAt, 0);
  assert.ok(!/PRIVATE|SECRET/.test(doctorSupportSummary(report)));
});
