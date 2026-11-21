/** Display identity only; raw versions remain available for update comparisons. */
export function buildIdentity(
  metadata:
    | { version?: string; prerelease?: string; branch?: string; commit?: string }
    | null
    | undefined,
  unknown = 'Unknown',
): string {
  if (!metadata) return unknown;
  const version = metadata.version || '';
  const prerelease = metadata.prerelease || '';
  const beta =
    metadata.branch?.startsWith('beta/') ||
    /(?:^|[-.])beta(?:$|[-.])/i.test(version) ||
    /(?:^|[-.])beta(?:$|[-.])/i.test(prerelease);
  if (beta) {
    const commit = metadata.commit || '';
    return /^[0-9a-f]{7,40}$/i.test(commit)
      ? `Beta · ${commit.slice(0, 8)}`
      : 'Beta · commit unavailable';
  }
  if (!version) return unknown;
  return prerelease && !version.endsWith(`-${prerelease}`) ? `${version}-${prerelease}` : version;
}
