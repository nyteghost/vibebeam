import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  canLaunchHostUpdate,
  hostManifestFromRelease,
  hostReleaseOrder,
  hostUpdateUri,
  selectHostRelease,
  verifyHostManifest,
  type HostRelease,
} from '../utils/hostUpdates.ts';
function fixture(channel: 'main' | 'beta' = 'beta') {
  const tag = `host-${channel}-12345-1`;
  const data = {
    schema_version: 1,
    repository: 'nyteghost/vibebeam',
    branch: channel === 'beta' ? 'beta/host-download-20261028' : 'master',
    channel,
    architecture: 'x86_64',
    source_commit: 'b'.repeat(40),
    workflow_run_id: 12345,
    run_attempt: 1,
    package: 'vibebeam-2.0.1-9-x86_64.pkg.tar.zst',
    package_version: '2.0.1-9',
    size: 20,
    sha256: 'a'.repeat(64),
  };
  const release: HostRelease = {
    tag_name: tag,
    target_commitish: data.source_commit,
    prerelease: channel === 'beta',
    draft: false,
    body: `Notes\n<!-- host-update: ${JSON.stringify(data)} -->`,
    assets: [
      {
        name: data.package,
        size: 20,
        browser_download_url: `https://github.com/nyteghost/vibebeam/releases/download/${tag}/${data.package}`,
      },
    ],
  };
  return { release, data };
}
test('Main and Beta never cross-select releases', () => {
  const beta = fixture('beta').release,
    main = fixture('main').release;
  assert.equal(selectHostRelease([beta], 'main'), null);
  assert.equal(selectHostRelease([main], 'beta'), null);
  assert.equal(selectHostRelease([beta, main], 'main'), main);
  assert.equal(hostReleaseOrder({ ...beta, draft: true }, 'beta'), null);
  assert.equal(hostReleaseOrder({ ...beta, tag_name: 'tv-beta-11001' }, 'beta'), null);
});
test('publisher metadata is bounded, singular and tied to source and package', () => {
  for (const channel of ['beta', 'main'] as const) {
    const { release, data } = fixture(channel);
    assert.equal(
      verifyHostManifest(release, hostManifestFromRelease(release), channel).commit,
      data.source_commit,
    );
    assert.throws(() =>
      verifyHostManifest(release, { ...data, source_commit: 'c'.repeat(40) }, channel),
    );
    assert.throws(() => verifyHostManifest(release, { ...data, branch: 'attacker' }, channel));
    assert.throws(() => verifyHostManifest(release, { ...data, size: true }, channel));
    assert.throws(() => verifyHostManifest({ ...release, assets: [] }, data, channel));
    assert.throws(() =>
      hostManifestFromRelease({ ...release, body: `${release.body}${release.body}` }),
    );
  }
  assert.throws(() => hostManifestFromRelease({ ...fixture().release, body: '' }));
});
test('desktop handoff is restricted to supported local hosts and literal channels', () => {
  assert.equal(canLaunchHostUpdate('127.0.0.1', 'linux', 'x86_64'), true);
  assert.equal(canLaunchHostUpdate('10.200.1.149', 'linux', 'x86_64'), false);
  assert.equal(canLaunchHostUpdate('localhost', 'windows', 'x86_64'), false);
  assert.equal(canLaunchHostUpdate('localhost', 'linux', 'aarch64'), false);
  assert.equal(hostUpdateUri('beta'), 'vibebeam-update://beta');
  assert.equal(hostUpdateUri('main'), 'vibebeam-update://main');
  assert.throws(() => hostUpdateUri('beta?command=evil' as 'beta'));
});
