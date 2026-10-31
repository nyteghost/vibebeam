import assert from 'node:assert/strict';
import test from 'node:test';
import { recommendQuality, type QualitySignal } from '../utils/vibeIntelligence.ts';

const sustained = (overrides: Partial<QualitySignal> = {}): QualitySignal[] =>
  Array.from({ length: 10 }, (_, index) => ({
    at: index * 2000, fps: 60, targetFps: 60, rttMs: 20,
    packetLossPercent: 0, droppedFramesPercent: 0, ...overrides,
  }));

test('off mode produces no advice', () => {
  assert.deepEqual(recommendQuality('off', sustained({ packetLossPercent: 10 }), 18000), []);
});
test('missing metrics do not create false recommendations', () => {
  assert.deepEqual(recommendQuality('smart-local', sustained().map(({ at }) => ({ at })), 18000), []);
});
test('short or stale observations do not create recommendations', () => {
  assert.deepEqual(recommendQuality('smart-local', sustained({ rttMs: 200 }).slice(0, 4), 6000), []);
  assert.deepEqual(recommendQuality('smart-local', sustained({ rttMs: 200 }), 300000), []);
});
test('sustained packet loss suggests a manual change without applying it', () => {
  const result = recommendQuality('smart-local', sustained({ packetLossPercent: 4 }), 18000);
  assert.equal(result.length, 1);
  assert.equal(result[0].id, 'packet-loss');
  assert.equal(result[0].requiresConfirmation, true);
});
test('isolated spikes do not trigger advice', () => {
  const samples = sustained();
  samples[4].rttMs = 400;
  assert.deepEqual(recommendQuality('smart-local', samples, 18000), []);
});
test('low fps and high latency can be independently detected', () => {
  const result = recommendQuality('smart-local', sustained({ fps: 30, rttMs: 100 }), 18000);
  assert.deepEqual(result.map((r) => r.id), ['high-latency', 'low-fps']);
});

test('replayed timestamps and gaps never establish a sustained trend', () => {
  const samples = sustained({ rttMs: 200 });
  assert.deepEqual(recommendQuality('smart-local', [...Array(10).fill(samples[0]), samples[9]], 18000), []);
  assert.deepEqual(recommendQuality('smart-local', [...samples.slice(0, 5), ...samples.slice(5).map((s) => ({ ...s, at: s.at + 20000 }))], 38000), []);
});
test('recent samples do not revive stale or too-short metric evidence', () => {
  const samples = sustained({ rttMs: 200 });
  assert.deepEqual(recommendQuality('smart-local', samples, 28001), []);
  // Overall window is long enough, but the valid RTT observations span only 14s.
  samples[0].rttMs = undefined;
  samples[9].rttMs = undefined;
  assert.deepEqual(recommendQuality('smart-local', samples, 18000), []);
  const freshMissing = sustained().map(({ at }) => ({ at: at + 24000 }));
  assert.deepEqual(recommendQuality('smart-local', [...sustained({ rttMs: 200 }), ...freshMissing], 42000), []);
});
test('invalid percentages and non-finite values are missing data', () => {
  for (const invalid of [-1, 101, Infinity, NaN]) {
    assert.deepEqual(recommendQuality('smart-local', sustained({ packetLossPercent: invalid, droppedFramesPercent: invalid }), 18000), []);
  }
  assert.deepEqual(recommendQuality('smart-local', sustained({ rttMs: Infinity }), 18000), []);
  assert.deepEqual(recommendQuality('smart-local', sustained({ fps: NaN }), 18000), []);
});
test('ordering is irrelevant and future samples cannot contribute evidence', () => {
  const samples = sustained({ rttMs: 100 });
  assert.deepEqual(recommendQuality('smart-local', [...samples].reverse(), 18000), recommendQuality('smart-local', samples, 18000));
  assert.deepEqual(recommendQuality('smart-local', samples, 8000), []);
  assert.deepEqual(recommendQuality('smart-local', samples, NaN), []);
});
test('read-only advice does not mutate caller samples, even with Local AI fallback', () => {
  const samples = sustained({ packetLossPercent: 3 });
  const before = structuredClone(samples);
  assert.deepEqual(recommendQuality('local-ai', samples, 18000), recommendQuality('smart-local', samples, 18000));
  assert.deepEqual(samples, before);
});
