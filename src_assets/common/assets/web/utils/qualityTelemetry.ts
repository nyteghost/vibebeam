import type { QualitySignal } from './vibeIntelligence';

export interface SessionQuality {
  key: string;
  label: string;
  samples: QualitySignal[];
}
interface Baseline {
  at: number;
  uptime: number;
  frames: number;
  target: number;
  samples: QualitySignal[];
}
const valid = (value: unknown, min = 0, max = Number.MAX_SAFE_INTEGER): value is number =>
  typeof value === 'number' && Number.isFinite(value) && value >= min && value <= max;

/** Read-only host frame-rate observations. Never infer RTT/loss from host counters. */
export class QualityTelemetry {
  private baselines = new Map<string, Baseline>();

  clear(): void { this.baselines.clear(); }

  observe(payload: unknown, now: number): SessionQuality[] {
    const sessions = (payload as { sessions?: unknown } | null)?.sessions;
    if (!valid(now) || !Array.isArray(sessions) || sessions.length > 128 ||
        (payload as { status?: unknown }).status === false) {
      this.clear();
      return [];
    }
    const next = new Map<string, Baseline>();
    const results = new Map<string, SessionQuality>();
    const seen = new Set<string>();
    for (const session of sessions) {
      if (!session || typeof session !== 'object') continue;
      const { uuid, protocol, uptime_seconds: uptime, frames_sent: frames, target_fps: target } = session;
      if (typeof uuid !== 'string' || !uuid.trim() || uuid.length > 128 ||
          !['rtsp', 'webrtc'].includes(protocol) || !valid(uptime) ||
          !valid(frames) || !Number.isSafeInteger(frames) || !valid(target, 1, 1000)) continue;
      const key = `${protocol}:${uuid}`;
      // An ambiguous duplicate identity cannot contribute evidence.
      if (seen.has(key)) { next.delete(key); results.delete(key); continue; }
      seen.add(key);
      const previous = this.baselines.get(key);
      let samples: QualitySignal[] = [];
      if (previous && target === previous.target) {
        const elapsed = (now - previous.at) / 1000;
        const duration = uptime - previous.uptime;
        // Reset on page suspension, counter reset, stale/replayed uptime or clock jumps.
        if (elapsed > 0 && elapsed <= 10 && duration > 0 && duration <= 10 &&
            Math.abs(duration - elapsed) <= Math.max(2, elapsed * 0.5) && frames >= previous.frames) {
          const fps = (frames - previous.frames) / duration;
          if (valid(fps, 0, 1000)) {
            samples = [...previous.samples, { at: now, fps, targetFps: target }]
              .filter((sample) => now - sample.at <= 120_000).slice(-24);
          }
        }
      }
      next.set(key, { at: now, uptime, frames, target, samples });
      const name = typeof session.device_name === 'string' && session.device_name.trim()
        ? session.device_name : typeof session.client_name === 'string' && session.client_name.trim()
          ? session.client_name : 'Unnamed client';
      results.set(key, { key, label: `${name.slice(0, 128)} (${protocol.toUpperCase()})`, samples });
    }
    this.baselines = next;
    return [...results.values()];
  }
}
