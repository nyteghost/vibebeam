import { expect, localRoute, test, type Page } from './fixtures';

const metadata = {
  status: true, platform: 'linux', version: '2.0.1-beta.1', commit: 'a'.repeat(40),
  encoder_status: { state: 'ready', h264: true },
  capture_status: { configured_backend: 'kms', observed_backend: 'unknown', managed_event_driven: false, virtual_display_configured: true },
  virtual_display: { capable: true, ready: true },
  hostname: 'PRIVATE-HOST', token: 'SECRET-TOKEN', configPath: '/home/private',
};
const idle = { status: true, activeSessions: 0, appRunning: false, lastEncoderProbeFailed: false };

async function host(page: Page) {
  const state = { metadata: structuredClone(metadata) as unknown, session: { ...idle, appName: 'PRIVATE-APP' } as unknown,
    failed: false, mutations: [] as string[] };
  await localRoute(page, '**/api/**', async (route) => {
    const request = route.request();
    const path = new URL(request.url()).pathname;
    // Vite also serves /v2/api/client.ts; only mock actual host API calls.
    if (!path.startsWith('/api/')) { await route.fallback(); return; }
    if (request.method() !== 'GET') state.mutations.push(path);
    let body: unknown = { status: true };
    if (path === '/api/auth/status') body = { authenticated: true, credentials_configured: true, login_required: false };
    else if (path === '/api/configLocale') body = { locale: 'en' };
    else if (path === '/api/metadata' || path === '/api/session/status') {
      if (state.failed) { await route.fulfill({ status: 503, json: { error: 'SECRET-SERVER-PATH' } }); return; }
      body = path === '/api/metadata' ? state.metadata : state.session;
    }
    await route.fulfill({ json: body });
  });
  return state;
}

test('doctor distinguishes configured capture and unknown network without mutations on desktop and mobile', async ({ page }) => {
  const state = await host(page);
  await page.goto('/v2/doctor');
  await expect(page.getByRole('heading', { level: 1, name: 'Connection Doctor' })).toBeVisible();
  await expect(page.locator('[data-check="capture"]')).toContainText('Configuration alone does not prove capture is working');
  await expect(page.locator('[data-check="encoder"]')).toContainText('NVENC is not verified');
  await expect(page.locator('[data-check="network"]')).toHaveAttribute('data-state', 'unknown');
  await page.setViewportSize({ width: 390, height: 844 });
  await expect(page.getByRole('button', { name: 'Run checks again' })).toBeVisible();
  const preview = page.getByRole('textbox', { name: 'Preview support summary' });
  const summary = await preview.inputValue();
  expect(JSON.parse(summary).host.version).toBe('2.0.1-beta.1');
  expect(summary).not.toMatch(/PRIVATE|SECRET|\/home/);
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true);
  expect(state.mutations).toEqual([]);
});

test('a failed refresh replaces older positive evidence with unknown results', async ({ page }) => {
  const state = await host(page);
  await page.goto('/v2/doctor');
  await expect(page.locator('[data-check="api"]')).toHaveAttribute('data-state', 'reported');
  state.failed = true;
  await page.getByRole('button', { name: 'Run checks again' }).click();
  await expect(page.locator('[data-check="api"]')).toHaveAttribute('data-state', 'unknown');
  await expect(page.locator('[data-check="encoder"]')).toHaveAttribute('data-state', 'unknown');
  await expect(page.locator('[data-check="sessions"]')).toHaveAttribute('data-state', 'unknown');
  expect(await page.getByRole('textbox', { name: 'Preview support summary' }).inputValue()).not.toContain('SECRET');
  expect(state.mutations).toEqual([]);
});

test('copy uses only the previewed summary and clipboard failure offers manual text', async ({ page }) => {
  await host(page);
  await page.addInitScript(() => {
    Object.defineProperty(navigator, 'clipboard', { value: { writeText: async (value: string) => {
      (window as unknown as { copiedSummary: string }).copiedSummary = value;
    } }, configurable: true });
  });
  await page.goto('/v2/doctor');
  const preview = page.getByRole('textbox', { name: 'Preview support summary' });
  await expect(preview).toBeVisible();
  await page.getByRole('button', { name: 'Copy support summary' }).click();
  await expect(page.getByText('Support summary copied. Nothing was uploaded.', { exact: true })).toBeVisible();
  expect(await page.evaluate(() => (window as unknown as { copiedSummary: string }).copiedSummary)).toBe(await preview.inputValue());
  await page.evaluate(() => { Object.defineProperty(navigator, 'clipboard', { value: undefined, configurable: true }); });
  await page.getByRole('button', { name: 'Copy support summary' }).click();
  await expect(page.getByText(/Clipboard unavailable/)).toBeVisible();
  await expect(preview).toBeFocused();
});

test('encoder failures remain visible during an active stream', async ({ page }) => {
  const state = await host(page);
  state.session = { ...idle, activeSessions: 1, appRunning: true, lastEncoderProbeFailed: true };
  await page.goto('/v2/doctor');
  await expect(page.locator('[data-check="encoder"]')).toHaveAttribute('data-state', 'attention');
  await expect(page.locator('[data-check="sessions"]')).toContainText('1 active stream sessions');
  expect(state.mutations).toEqual([]);
});
