export type HostUpdateChannel = 'beta' | 'main';
export interface HostRelease {
  tag_name: string;
  target_commitish: string;
  body?: string;
  draft: boolean;
  prerelease: boolean;
  assets: Array<{ name: string; size: number; browser_download_url: string }>;
}
export interface HostUpdate {
  commit: string;
  version: string;
  tag: string;
  package: string;
}
const repo = 'nyteghost/vibebeam';
export function hostReleaseOrder(
  release: HostRelease,
  channel: HostUpdateChannel,
): number[] | null {
  const match = new RegExp(`^host-${channel}-(\\d+)-(\\d+)$`).exec(release.tag_name);
  if (!match || release.draft || release.prerelease !== (channel === 'beta')) return null;
  const order = match.slice(1).map(Number);
  return order.every((n) => Number.isSafeInteger(n) && n > 0) ? order : null;
}
export function selectHostRelease(
  releases: HostRelease[],
  channel: HostUpdateChannel,
): HostRelease | null {
  return (
    releases
      .filter((r) => hostReleaseOrder(r, channel))
      .sort((a, b) => {
        const aa = hostReleaseOrder(a, channel)!;
        const bb = hostReleaseOrder(b, channel)!;
        return bb[0] - aa[0] || bb[1] - aa[1];
      })[0] ?? null
  );
}
export function verifyHostManifest(
  release: HostRelease,
  data: Record<string, unknown>,
  channel: HostUpdateChannel,
): HostUpdate {
  const order = hostReleaseOrder(release, channel);
  const branch =
    channel === 'beta'
      ? data.branch === 'beta/host-download-20261028'
      : ['main', 'master'].includes(String(data.branch));
  if (
    !order ||
    data.schema_version !== 1 ||
    data.repository !== repo ||
    !branch ||
    data.channel !== channel ||
    data.architecture !== 'x86_64' ||
    String(data.workflow_run_id) !== String(order[0]) ||
    String(data.run_attempt) !== String(order[1]) ||
    typeof data.source_commit !== 'string' ||
    !/^[0-9a-f]{40}$/.test(data.source_commit) ||
    data.source_commit !== release.target_commitish ||
    typeof data.package !== 'string' ||
    !/^vibebeam-[A-Za-z0-9_.+-]+-x86_64\.pkg\.tar\.zst$/.test(data.package) ||
    typeof data.package_version !== 'string' ||
    !data.package_version ||
    typeof data.size !== 'number' ||
    !Number.isSafeInteger(data.size) ||
    data.size <= 0 ||
    data.size > 512 * 1024 * 1024 ||
    typeof data.sha256 !== 'string' ||
    !/^[0-9a-f]{64}$/.test(data.sha256)
  )
    throw new Error('Unverified host update');
  const url = `https://github.com/${repo}/releases/download/${release.tag_name}/${data.package}`;
  if (
    !release.assets.some(
      (a) => a.name === data.package && a.size === data.size && a.browser_download_url === url,
    )
  )
    throw new Error('Missing verified package');
  return {
    commit: data.source_commit,
    version: data.package_version,
    tag: release.tag_name,
    package: data.package,
  };
}
export function canLaunchHostUpdate(
  hostname: string,
  platform?: string,
  architecture?: string,
): boolean {
  return (
    ['localhost', '127.0.0.1', '[::1]', '::1'].includes(hostname) &&
    platform === 'linux' &&
    architecture === 'x86_64'
  );
}
export function hostUpdateUri(channel: HostUpdateChannel): string {
  if (channel !== 'beta' && channel !== 'main') throw new Error('Invalid update channel');
  return `vibebeam-update://${channel}`;
}

// GitHub release assets do not expose CORS headers. Read the publisher's bounded
// metadata copy from the public release API; the native updater verifies the asset again.
export function hostManifestFromRelease(release: HostRelease): Record<string, unknown> {
  const body = release.body ?? '';
  const marker = '<!-- host-update: ';
  const start = body.indexOf(marker);
  const end = body.indexOf(' -->', start + marker.length);
  if (
    start < 0 ||
    end < 0 ||
    end - start > 16384 ||
    body.indexOf(marker, start + marker.length) >= 0
  )
    throw new Error('Missing or ambiguous update metadata');
  const data = JSON.parse(body.slice(start + marker.length, end));
  if (!data || typeof data !== 'object' || Array.isArray(data))
    throw new Error('Invalid update metadata');
  return data;
}
