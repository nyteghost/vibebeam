import assert from 'node:assert/strict';
import test from 'node:test';
import { intelligencePreferencesKey, readIntelligencePreferences, writeIntelligencePreferences } from '../utils/intelligencePreferences.ts';

function storage(initial: string | null = null) {
  let saved = initial;
  return {
    getItem: (key: string) => { assert.equal(key, intelligencePreferencesKey); return saved; },
    setItem: (key: string, value: string) => { assert.equal(key, intelligencePreferencesKey); saved = value; },
  };
}
test('explicit Off and scoped dismissals survive a reload', () => {
  const store = storage();
  const preferences = { mode: 'off' as const, dismissed: ['packet-loss' as const] };
  assert.equal(writeIntelligencePreferences(store, preferences), true);
  assert.deepEqual(readIntelligencePreferences(store), preferences);
});
test('malformed, unavailable or unsupported preferences fail safely', () => {
  for (const value of [null, '{broken', 'null', '7', '[]', '{"mode":"cloud"}']) {
    assert.deepEqual(readIntelligencePreferences(storage(value)), { mode: 'smart-local', dismissed: [] });
  }
  const blocked = { getItem() { throw new Error('blocked'); }, setItem() { throw new Error('quota'); } };
  assert.deepEqual(readIntelligencePreferences(blocked), { mode: 'smart-local', dismissed: [] });
  assert.equal(writeIntelligencePreferences(blocked, { mode: 'off', dismissed: [] }), false);
});
test('unavailable AI falls back to rules and only known dismissal IDs are retained', () => {
  assert.deepEqual(readIntelligencePreferences(storage(JSON.stringify({
    mode: 'local-ai', dismissed: ['packet-loss', 'packet-loss', 0, null, 'unknown', 'low-fps'], token: 'never-retain',
  }))), { mode: 'smart-local', dismissed: ['packet-loss', 'low-fps'] });
});
