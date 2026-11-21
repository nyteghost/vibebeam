import { expect, localRoute, test, type Page } from './fixtures';
async function setup(page: Page) {
  const session = { status: true, activeSessions: 0, appRunning: false };
  await localRoute(page, '**/api/**', async (route) => {
    const path = new URL(route.request().url()).pathname;
    if (!path.startsWith('/api/')) { await route.fallback(); return; }
    let body: unknown = { status: true };
    if (path === '/api/auth/status') body = { authenticated: true, credentials_configured: true, login_required: false };
    if (path === '/api/configLocale') body = { locale: 'en' };
    if (path === '/api/metadata') body = { status: true, platform: 'linux', architecture: 'x86_64', version: '2.0.1-beta.1', commit: 'a'.repeat(40) };
    if (path === '/api/session/status') body = session;
    await route.fulfill({ json: body });
  });
  const tag = 'host-beta-12345-1';
  const data = { schema_version: 1, repository: 'nyteghost/vibebeam', branch: 'beta/host-download-20261028', channel: 'beta', architecture: 'x86_64', source_commit: 'b'.repeat(40), workflow_run_id: 12345, run_attempt: 1, package: 'vibebeam-2.0.1-9-x86_64.pkg.tar.zst', package_version: '2.0.1-9', size: 20, sha256: 'a'.repeat(64) };
  await page.route('https://api.github.com/repos/nyteghost/vibebeam/releases?**', async (route) => route.fulfill({ json: [{ tag_name: tag, target_commitish: data.source_commit, prerelease: true, draft: false, body: `<!-- host-update: ${JSON.stringify(data)} -->`, assets: [{ name: data.package, size: 20, browser_download_url: `https://github.com/nyteghost/vibebeam/releases/download/${tag}/${data.package}` }] }] }));
  await page.goto('/v2/maintenance');
  return session;
}
test('channel selection persists and Main never falls back to Beta', async ({ page }) => {
  await setup(page);
  const controls = page.locator('.host-update-controls');
  await controls.getByRole('button', { name: 'Check for update', exact: true }).click();
  await expect(controls).toContainText('Beta · bbbbbbbb');
  await expect(controls.getByRole('button', { name: 'Update', exact: true })).toBeEnabled();
  await controls.getByLabel('Update channel').selectOption('main');
  await expect(controls.getByRole('button', { name: 'Update', exact: true })).toBeDisabled();
  await controls.getByRole('button', { name: 'Check for update', exact: true }).click();
  await expect(controls).toContainText('No verified Main package');
  await page.reload();
  await expect(page.getByLabel('Update channel')).toHaveValue('main');
});
test('confirmation can be cancelled and newly active sessions block installation', async ({ page }) => {
  const session = await setup(page);
  const controls = page.locator('.host-update-controls');
  await controls.getByRole('button', { name: 'Check for update', exact: true }).click();
  const update = controls.getByRole('button', { name: 'Update', exact: true });
  await expect(update).toBeEnabled();
  await update.click();
  await page.getByRole('dialog', { name: 'Install host update?' }).getByRole('button', { name: 'Cancel', exact: true }).click();
  await expect(page.getByRole('dialog', { name: 'Install host update?' })).not.toBeVisible();
  await update.click();
  session.activeSessions = 1;
  await page.getByRole('button', { name: 'Open installer', exact: true }).click();
  await expect(controls).toContainText('Disconnect streaming clients');
  await expect(page).toHaveURL(/\/v2\/maintenance$/);
});
