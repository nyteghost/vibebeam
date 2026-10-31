/**
 * Offline-first, read-only quality advisor.
 * No telemetry upload, settings mutation, or model execution occurs here.
 * Callers must implement a separate user-confirmed apply/undo transaction.
 */
export type IntelligenceMode = 'off' | 'smart-local' | 'local-ai';
export type QualitySignal = {
  at: number;
  fps?: number;
  targetFps?: number;
  rttMs?: number;
  packetLossPercent?: number;
  droppedFramesPercent?: number;
};
export type QualityAdvice = {
  id: 'high-latency' | 'packet-loss' | 'dropped-frames' | 'low-fps';
  summary: string;
  evidence: string;
  suggestion: string;
  confidence: 'moderate' | 'high';
  requiresConfirmation: true;
};
const MIN_SAMPLES = 6;
const MIN_WINDOW_MS = 15_000;
const MAX_WINDOW_MS = 120_000;
const finite = (value: number | undefined, min: number, max: number): value is number =>
  typeof value === 'number' && Number.isFinite(value) && value >= min && value <= max;

export function recommendQuality(
  mode: IntelligenceMode,
  samples: readonly QualitySignal[],
  now: number,
): QualityAdvice[] {
  if (mode === 'off' || !Number.isFinite(now)) return [];
  const window = samples.filter(
    (s) => finite(s.at, 0, Number.MAX_SAFE_INTEGER) &&
      s.at <= now && now - s.at <= MAX_WINDOW_MS,
  ).sort((a, b) => a.at - b.at);
  if (window.length < MIN_SAMPLES || window[window.length - 1].at - window[0].at < MIN_WINDOW_MS)
    return [];
  const rules: Array<{
    id: QualityAdvice['id'];
    title: string;
    suggestion: string;
    unit: string;
    metric: (s: QualitySignal) => number | undefined;
    threshold: number;
    comparison: (n: number, threshold: number) => boolean;
  }> = [
    { id: 'packet-loss', title: 'Sustained packet loss', suggestion: 'Consider lowering the stream bitrate.', unit: '%',
      metric: (s) => s.packetLossPercent, threshold: 2, comparison: (n, t) => n >= t },
    { id: 'high-latency', title: 'Sustained high latency', suggestion: 'Check network congestion before changing quality.', unit: ' ms',
      metric: (s) => s.rttMs, threshold: 80, comparison: (n, t) => n >= t },
    { id: 'dropped-frames', title: 'Sustained dropped frames', suggestion: 'Consider a lower resolution or frame rate.', unit: '%',
      metric: (s) => s.droppedFramesPercent, threshold: 5, comparison: (n, t) => n >= t },
    { id: 'low-fps', title: 'Frame rate below target', suggestion: 'Consider a lower target FPS or quality preset.', unit: '% of target',
      metric: (s) => finite(s.fps, 0, 1000) && finite(s.targetFps, 1, 1000) ? 100 * s.fps / s.targetFps : undefined,
      threshold: 80, comparison: (n, t) => n <= t },
  ];
  const advice: QualityAdvice[] = [];
  for (const rule of rules) {
    const values = window.map(rule.metric).filter((n): n is number => finite(n, 0, 100000));
    // Missing signals are unknown, never interpreted as zero or as a problem.
    if (values.length < MIN_SAMPLES || values.length < window.length * 0.8) continue;
    const elevated = values.filter((n) => rule.comparison(n, rule.threshold)).length;
    if (elevated / values.length < 0.8) continue;
    const mean = values.reduce((sum, n) => sum + n, 0) / values.length;
    advice.push({
      id: rule.id,
      summary: rule.title,
      evidence: `${elevated}/${values.length} recent samples; mean ${mean.toFixed(1)}${rule.unit}.`,
      suggestion: rule.suggestion,
      confidence: elevated === values.length && values.length >= 10 ? 'high' : 'moderate',
      requiresConfirmation: true,
    });
  }
  return advice;
}
