import assert from 'node:assert/strict';
import test from 'node:test';
import { QualityTelemetry } from '../utils/qualityTelemetry.ts';
import { recommendQuality } from '../utils/vibeIntelligence.ts';

const stream = (uuid: string, seconds: number, fps = 30, target = 60) => ({
  uuid, protocol: 'rtsp', device_name: 'Client', target_fps: target,
  uptime_seconds: seconds, frames_sent: seconds * fps,
});
test('host counter deltas establish per-session FPS without inventing network metrics', () => {
  const sampler = new QualityTelemetry();
  let result = sampler.observe({ sessions: [stream('one', 0)] }, 1000);
  assert.equal(result[0].samples.length, 0);
  for (let i = 1; i <= 6; i++) result = sampler.observe({ sessions: [stream('one', i * 5)] }, 1000 + i * 5000);
  assert.equal(result[0].samples.length, 6);
  assert.deepEqual(result[0].samples[5], { at: 31000, fps: 30, targetFps: 60 });
  assert.deepEqual(recommendQuality('smart-local', result[0].samples, 31000).map((item) => item.id), ['low-fps']);
});
test('different sessions are isolated and ended sessions are discarded', () => {
  const sampler = new QualityTelemetry();
  sampler.observe({ sessions: [stream('slow', 0), stream('fast', 0, 60)] }, 0);
  const result = sampler.observe({ sessions: [stream('slow', 5), stream('fast', 5, 60)] }, 5000);
  assert.equal(result[0].samples[0].fps, 30);
  assert.equal(result[1].samples[0].fps, 60);
  assert.deepEqual(sampler.observe({ sessions: [] }, 10000), []);
  assert.equal(sampler.observe({ sessions: [stream('slow', 15)] }, 15000)[0].samples.length, 0);
});
test('replay, backwards counters, target changes and suspension reset the window', () => {
  for (const next of [stream('one', 5), stream('one', 4), stream('one', 10, 1), stream('one', 10, 30, 120)]) {
    const sampler = new QualityTelemetry();
    sampler.observe({ sessions: [stream('one', 0)] }, 0);
    sampler.observe({ sessions: [stream('one', 5)] }, 5000);
    assert.equal(sampler.observe({ sessions: [next] }, 10000)[0].samples.length, 0);
  }
  const sampler = new QualityTelemetry();
  sampler.observe({ sessions: [stream('one', 0)] }, 0);
  assert.equal(sampler.observe({ sessions: [stream('one', 30)] }, 30000)[0].samples.length, 0);
});
test('malformed responses, ambiguous IDs and explicit clear do not retain old evidence', () => {
  const sampler = new QualityTelemetry();
  for (const payload of [null, {}, { status: false, sessions: [] }, { sessions: [{ uuid: 'one', frames_sent: NaN }] }, { sessions: [stream('one', 5), stream('one', 5)] }]) {
    sampler.observe({ sessions: [stream('one', 0)] }, 0);
    assert.deepEqual(sampler.observe(payload, 5000), []);
    assert.equal(sampler.observe({ sessions: [stream('one', 10)] }, 10000)[0].samples.length, 0);
  }
  sampler.clear();
  assert.equal(sampler.observe({ sessions: [stream('one', 15)] }, 15000)[0].samples.length, 0);
});
test('observation history is bounded and zero FPS is a measured stall', () => {
  const sampler = new QualityTelemetry();
  let result;
  for (let i = 0; i < 100; i++) result = sampler.observe({ sessions: [stream('one', i * 5, 0)] }, i * 5000);
  assert.equal(result![0].samples.length, 24);
  assert.equal(result![0].samples[0].fps, 0);
});
