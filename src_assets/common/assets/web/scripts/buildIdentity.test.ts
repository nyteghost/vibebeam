import { test } from 'node:test';
import assert from 'node:assert/strict';
import { buildIdentity } from '../utils/buildIdentity.ts';

test('beta identity uses real commit for each supported metadata format', () => {
  for (const meta of [
    { version: '2.0.1-beta-host-download' },
    { version: '2.0.1', prerelease: 'beta-host-download' },
    { version: '2.0.1', branch: 'beta/host-download-20261028' },
  ])
    assert.equal(
      buildIdentity({ ...meta, commit: 'b35c439e' + 'a'.repeat(32) }),
      'Beta · b35c439e',
    );
});
test('unknown commit is explicit and stable labels do not duplicate suffixes', () => {
  assert.equal(buildIdentity({ version: '2.0.1-beta.9' }), 'Beta · commit unavailable');
  assert.equal(
    buildIdentity({ version: '2.0.1-beta.9', commit: '<invalid>' }),
    'Beta · commit unavailable',
  );
  assert.equal(buildIdentity({ version: '2.0.1' }), '2.0.1');
  assert.equal(buildIdentity({ version: '2.0.1-rc.1', prerelease: 'rc.1' }), '2.0.1-rc.1');
  assert.equal(buildIdentity(null, 'Unavailable'), 'Unavailable');
});
