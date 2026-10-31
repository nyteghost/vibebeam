import { localRoute, expect, test, type Page } from './fixtures';

async function host(
  page: Page,
  platform = 'linux',
  result: unknown = {
    status: true,
    virtual_displays_removed: true,
    topology_restored: true,
    physical_display_recovered: true,
  },
) {
  const calls: Array<{ body: unknown; csrf: string | undefined }> = [];
  await localRoute(page, '**/api/**', async (route) => {
    const request = route.request();
    const path = new URL(request.url()).pathname;
    if (!path.startsWith('/api/')) {
      await route.fallback();
      return;
    }
    let body: unknown = { status: true };
    if (path === '/api/auth/status') {
      body = { authenticated: true, credentials_configured: true, login_required: false };
    } else if (path === '/api/configLocale') body = { locale: 'en' };
    else if (path === '/api/csrf-token') body = { csrf_token: 'recovery-test-token' };
    else if (path === '/api/metadata') {
      body = { platform, version: '2.0.1-beta.1', encoder_status: { state: 'ready', h264: true } };
    } else if (path === '/api/session/status') {
      body = { status: true, activeSessions: 1, appRunning: true };
    } else if (path === '/api/host/info') body = { cpu_model: 'CPU', gpu_model: 'GPU' };
    else if (path === '/api/host/stats') body = { cpu_percent: 0, gpu_percent: 0 };
    else if (path === '/api/history/sessions/active') body = { sessions: [] };
    else if (path === '/api/display/terminate_virtual') {
      expect(request.method()).toBe('POST');
      calls.push({ body: request.postDataJSON(), csrf: request.headers()['x-csrf-token'] });
      await route.fulfill({ body: JSON.stringify(result), contentType: 'application/json' });
      return;
    }
    await route.fulfill({ json: body });
  });
  return calls;
}

test('host workspace exposes accessible health and refresh on desktop and mobile', async ({ page }) => {
  await host(page);
  await page.goto('/v2/');
  const status = page.locator('.workspace-status');
  await expect(status).toBeVisible();
  await expect(status.locator('a[href="/v2/stats"]')).toBeVisible();
  const refresh = status.locator('button.workspace-status__refresh');
  await expect(refresh).toHaveAttribute('aria-label', /.+/);
  await expect(refresh).toBeVisible();
  await refresh.click();
  await expect(status).toBeVisible();

  await page.setViewportSize({ width: 390, height: 844 });
  await expect(status).toBeVisible();
  await expect(refresh).toBeVisible();
  await expect(status.locator('a[href="/v2/stats"]')).toBeVisible();
});

async function confirmRecovery(page: Page) {
  await page.getByRole('button', { name: 'Terminate virtual screen', exact: true }).click();
  await expect(page.getByRole('dialog')).toContainText('Streams using them will lose video.');
  await page
    .getByRole('dialog')
    .getByRole('button', { name: 'Terminate virtual screen', exact: true })
    .click();
}

for (const platform of ['linux', 'windows']) {
  test(`${platform} dashboard introduces recovery and preserves the action after dismissal`, async ({
    page,
  }) => {
    const calls = await host(page, platform);
    await page.goto('/v2/');
    await expect(
      page.getByText('Know how to recover your PC screen', { exact: true }),
    ).toBeVisible();
    await page.getByRole('button', { name: 'Got it', exact: true }).click();
    await page.reload();
    await expect(
      page.getByRole('button', { name: 'Terminate virtual screen', exact: true }),
    ).toBeVisible();
    await expect(page.getByText('Know how to recover your PC screen', { exact: true })).toHaveCount(
      0,
    );
    expect(calls).toHaveLength(0);
    await confirmRecovery(page);
    await expect(page.getByRole('status')).toContainText(
      'saved physical display layout was restored',
    );
    expect(calls).toEqual([{ body: {}, csrf: 'recovery-test-token' }]);
  });
}

test('cancelling the recovery dialog never sends a termination request', async ({ page }) => {
  const calls = await host(page);
  await page.goto('/v2/');
  await page.getByRole('button', { name: 'Terminate virtual screen', exact: true }).click();
  await page.getByRole('dialog').getByRole('button', { name: 'Cancel', exact: true }).click();
  await expect(page.getByRole('dialog')).not.toBeVisible();
  expect(calls).toHaveLength(0);
});

test('unsupported hosts do not advertise display termination', async ({ page }) => {
  await host(page, 'macos');
  await page.goto('/v2/');
  await expect(page.getByRole('heading', { name: 'Host performance', exact: true })).toBeVisible();
  await expect(
    page.getByRole('button', { name: 'Terminate virtual screen', exact: true }),
  ).toHaveCount(0);
});

const responses = [
  {
    name: 'fallback recovery',
    result: { status: true, topology_restored: false, physical_display_recovered: true },
    message: 'a physical monitor is active',
  },
  {
    name: 'failed physical recovery',
    result: { status: true, topology_restored: false, physical_display_recovered: false },
    message: 'physical-display recovery could not be confirmed',
  },
  {
    name: 'unconfirmed helper dispatch',
    result: { status: true, restore_dispatched: true },
    message: 'physical-display recovery was requested',
  },
  {
    name: 'termination failure',
    result: { status: false, physical_display_recovered: true },
    message: 'termination could not be confirmed',
  },
  { name: 'missing confirmation', result: {}, message: 'termination could not be confirmed' },
  { name: 'empty response', result: null, message: 'termination could not be confirmed' },
  {
    name: 'contradictory removal result',
    result: { status: true, virtual_displays_removed: false },
    message: 'termination could not be confirmed',
  },
];
for (const { name, result, message } of responses) {
  test(`dashboard reports ${name} separately from virtual-screen removal`, async ({ page }) => {
    const calls = await host(page, 'linux', result);
    await page.goto('/v2/');
    await confirmRecovery(page);
    await expect(page.getByRole('status')).toContainText(message);
    expect(calls).toHaveLength(1);
  });
}

test('recovery is available even when dashboard metrics fail', async ({ page }) => {
  const calls = await host(page);
  await localRoute(page, '**/api/host/stats', (route) =>
    route.fulfill({ status: 503, json: { error: 'unavailable' } }),
  );
  await page.goto('/v2/');
  await confirmRecovery(page);
  await expect(page.getByRole('status')).toContainText(
    'saved physical display layout was restored',
  );
  expect(calls).toHaveLength(1);
});

test('network failures leave recovery available for another attempt', async ({ page }) => {
  const calls = await host(page);
  await localRoute(page, '**/api/display/terminate_virtual', (route) => route.abort());
  await page.goto('/v2/');
  await confirmRecovery(page);
  await expect(page.getByRole('status')).toContainText('termination could not be confirmed');
  await page.unroute('**/api/display/terminate_virtual');
  await confirmRecovery(page);
  await expect(page.getByRole('status')).toContainText(
    'saved physical display layout was restored',
  );
  expect(calls).toHaveLength(1);
});

test('an in-flight recovery cannot be cancelled or submitted twice', async ({ page }) => {
  await host(page);
  let count = 0;
  let finish: (() => void) | undefined;
  const pending = new Promise<void>((resolve) => {
    finish = resolve;
  });
  await localRoute(page, '**/api/display/terminate_virtual', async (route) => {
    count += 1;
    await pending;
    await route.fulfill({ json: { status: true, physical_display_recovered: true } });
  });
  await page.goto('/v2/');
  await confirmRecovery(page);
  await expect(
    page.getByRole('dialog').getByRole('button', { name: 'Recovering displays…' }),
  ).toBeDisabled();
  await expect(
    page.getByRole('dialog').getByRole('button', { name: 'Cancel', exact: true }),
  ).toBeDisabled();
  await page.keyboard.press('Escape');
  await expect(page.getByRole('dialog')).toBeVisible();
  expect(count).toBe(1);
  finish?.();
  await expect(page.getByRole('status')).toContainText('a physical monitor is active');
});

test('offline advisor saves Off with keyboard controls and makes no host mutations', async ({ page }) => {
  await host(page);
  const mutations: string[] = [];
  page.on('request', (request) => {
    if (new URL(request.url()).pathname.startsWith('/api/') && request.method() !== 'GET') mutations.push(request.url());
  });
  await page.goto('/v2/');
  await page.setViewportSize({ width: 390, height: 844 });
  const mode = page.getByRole('combobox', { name: 'Vibe Intelligence mode' });
  await expect(mode).toHaveValue('smart-local');
  await mode.focus();
  await mode.press('Home');
  await mode.press('Enter');
  await expect(mode).toHaveValue('off');
  await expect(page.getByText('Recommendations are turned off.', { exact: true })).toBeVisible();
  await page.reload();
  await expect(mode).toHaveValue('off');
  await expect(mode.locator('option[value="local-ai"]')).toHaveJSProperty('disabled', true);
  await mode.selectOption('smart-local');
  await expect(page.getByText(/No active stream measurements available yet/)).toBeVisible();
  expect(mutations).toEqual([]);
});

test('advisor restores dismissed types and handles unavailable browser storage', async ({ page }) => {
  await host(page);
  await page.addInitScript(() => {
    localStorage.setItem('vibebeam.intelligence.v1', JSON.stringify({ mode: 'smart-local', dismissed: ['packet-loss'] }));
    const setItem = Storage.prototype.setItem;
    Storage.prototype.setItem = function(key, value) {
      if (key === 'vibebeam.intelligence.v1') throw new Error('Storage disabled');
      return setItem.call(this, key, value);
    };
  });
  await page.goto('/v2/');
  await page.getByRole('button', { name: 'Restore dismissed suggestions', exact: true }).click();
  await expect(page.getByText(/Browser storage is unavailable/)).toBeVisible();
  await expect(page.getByText(/No active stream measurements available yet/)).toBeVisible();
  await page.getByRole('combobox', { name: 'Vibe Intelligence mode' }).selectOption('off');
  await expect(page.getByText('Recommendations are turned off.', { exact: true })).toBeVisible();
});

test('advisor uses live session counters, dismisses evidence and stops sampling when Off', async ({ page }) => {
  await page.clock.install();
  await host(page);
  let reads = 0;
  await localRoute(page, '**/api/history/sessions/active', async (route) => {
    const seconds = reads++ * 5;
    await route.fulfill({ json: { sessions: [{ uuid: 'slow-client', protocol: 'rtsp', device_name: 'Slow client',
      target_fps: 60, uptime_seconds: seconds, frames_sent: seconds * 30 }] } });
  });
  await page.goto('/v2/');
  const panel = page.locator('.intelligence-panel');
  await expect(panel.getByRole('combobox', { name: 'Quality guidance session' })).toHaveValue('rtsp:slow-client');
  for (let i = 1; i <= 6; i++) {
    await page.clock.runFor(5000);
    await expect(panel).toContainText(`${i} observations`);
  }
  await expect(panel).toContainText('Host output: 30.0 / 60 FPS');
  await expect(panel.getByRole('status')).toContainText('Frame rate below target');
  await panel.getByRole('button', { name: "Don't suggest these again for this host", exact: true }).click();
  await expect(panel.getByRole('status')).toHaveCount(0);
  await expect(panel.getByRole('button', { name: 'Restore dismissed suggestions' })).toBeVisible();
  await panel.getByRole('button', { name: 'Restore dismissed suggestions' }).click();
  await expect(panel.getByRole('status')).toContainText('Frame rate below target');
  await panel.getByRole('combobox', { name: 'Vibe Intelligence mode' }).selectOption('off');
  await expect(panel).toContainText('Recommendations are turned off.');
  const before = reads;
  await page.clock.runFor(15000);
  expect(reads).toBe(before);
});
