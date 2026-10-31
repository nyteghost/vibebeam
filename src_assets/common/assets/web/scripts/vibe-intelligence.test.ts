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
