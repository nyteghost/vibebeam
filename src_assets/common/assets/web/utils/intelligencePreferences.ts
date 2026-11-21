import type { IntelligenceMode, QualityAdvice } from './vibeIntelligence';

export interface IntelligencePreferences {
  mode: IntelligenceMode;
  dismissed: QualityAdvice['id'][];
}

// Browser storage is scoped to this host UI's origin (including its port).
// Store only explicit choices, never stream samples or host credentials.
export const intelligencePreferencesKey = 'vibebeam.intelligence.v1';
const adviceIds = new Set(['high-latency', 'packet-loss', 'dropped-frames', 'low-fps']);
type PreferenceStorage = Pick<Storage, 'getItem' | 'setItem'>;

export function readIntelligencePreferences(storage: PreferenceStorage): IntelligencePreferences {
  const defaults: IntelligencePreferences = { mode: 'smart-local', dismissed: [] };
  try {
    const value = JSON.parse(storage.getItem(intelligencePreferencesKey) ?? 'null');
    if (!value || typeof value !== 'object') return defaults;
    // Local AI is unavailable: transparently use rules without model execution.
    const mode = value.mode === 'off' ? 'off' : 'smart-local';
    const dismissed = Array.isArray(value.dismissed)
      ? [...new Set<QualityAdvice['id']>(value.dismissed.filter(
        (id: unknown) => typeof id === 'string' && adviceIds.has(id),
      ))] : [];
    return { mode, dismissed };
  } catch {
    return defaults;
  }
}

export function writeIntelligencePreferences(
  storage: PreferenceStorage, preferences: IntelligencePreferences,
): boolean {
  try {
    storage.setItem(intelligencePreferencesKey, JSON.stringify(preferences));
    return true;
  } catch {
    return false;
  }
}
