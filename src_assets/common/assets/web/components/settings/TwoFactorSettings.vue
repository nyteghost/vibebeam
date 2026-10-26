<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { ApiError, apiGet, apiPost } from '@/api/client';
import { AppButton, InlineAlert, StatusBadge } from '@/components/ui';
import qrcode from '@/utils/vendor/qrcode.mjs';

interface TwoFactorStatus {
  enabled: boolean;
  setup_pending: boolean;
  backup_codes_remaining: number;
}
interface SetupResponse {
  secret: string;
  uri: string;
}
interface EnableResponse {
  backup_codes: string[];
}

const status = ref<TwoFactorStatus | null>(null);
const setup = ref<SetupResponse | null>(null);
const backupCodes = ref<string[]>([]);
const code = ref('');
const busy = ref(false);
const error = ref('');
const copied = ref(false);
const confirmingOff = ref(false);

const qr = computed(() => {
  if (!setup.value) return null;
  try {
    const symbol = qrcode(0, 'M');
    symbol.addData(setup.value.uri);
    symbol.make();
    const size = symbol.getModuleCount();
    let path = '';
    for (let row = 0; row < size; row++) {
      for (let col = 0; col < size; col++) {
        if (symbol.isDark(row, col)) path += `M${col + 4},${row + 4}h1v1h-1z`;
      }
    }
    return { size: size + 8, path };
  } catch {
    return null;
  }
});

function describe(err: unknown, fallback: string): string {
  if (err instanceof ApiError) {
    const payload = err.payload as { error?: unknown } | undefined;
    if (payload && typeof payload.error === 'string' && payload.error) return payload.error;
  }
  return fallback;
}

async function load() {
  try {
    status.value = await apiGet<TwoFactorStatus>('/api/two-factor');
  } catch {
    error.value = 'Could not read the two-factor status from the host.';
  }
}

async function startSetup() {
  busy.value = true;
  error.value = '';
  code.value = '';
  try {
    setup.value = await apiPost<SetupResponse>('/api/two-factor/setup', {});
  } catch (err) {
    error.value = describe(err, 'Could not start the setup.');
  } finally {
    busy.value = false;
  }
}

function cancelSetup() {
  setup.value = null;
  code.value = '';
  error.value = '';
}

async function confirmSetup() {
  busy.value = true;
  error.value = '';
  try {
    const result = await apiPost<EnableResponse>('/api/two-factor/enable', { code: code.value });
    backupCodes.value = result.backup_codes;
    setup.value = null;
    code.value = '';
    await load();
  } catch (err) {
    error.value = describe(err, 'That code was not accepted.');
  } finally {
    busy.value = false;
  }
}

async function turnOff() {
  busy.value = true;
  error.value = '';
  try {
    await apiPost('/api/two-factor/disable', { code: code.value });
    code.value = '';
    confirmingOff.value = false;
    await load();
  } catch (err) {
    error.value = describe(err, 'That code was not accepted.');
  } finally {
    busy.value = false;
  }
}

async function copyBackup() {
  try {
    await navigator.clipboard.writeText(backupCodes.value.join('\n'));
    copied.value = true;
    window.setTimeout(() => (copied.value = false), 2500);
  } catch {
    copied.value = false;
  }
}

onMounted(load);
</script>

<template>
  <section class="two-factor" aria-labelledby="two-factor-title">
    <header class="two-factor__heading">
      <h3 id="two-factor-title">Require 2FA to connect</h3>
      <StatusBadge
        v-if="status"
        :label="status.enabled ? 'On' : 'Off'"
        :tone="status.enabled ? 'success' : 'neutral'"
      />
    </header>
    <p>
      When this is on, Vibelight asks for a six-digit code from your authenticator app every time
      someone starts or resumes a stream. Clients that cannot type a code (for example the stock
      iOS Moonlight app) need “Exempt from 2FA” turned on for that device in the Devices page.
    </p>

    <InlineAlert v-if="error" tone="danger" title="Two-factor">{{ error }}</InlineAlert>

    <template v-if="backupCodes.length">
      <InlineAlert tone="warning" title="Save your backup codes now">
        Each code works once if you lose your phone. They will not be shown again.
      </InlineAlert>
      <pre class="two-factor__codes">{{ backupCodes.join('\n') }}</pre>
      <div class="two-factor__actions">
        <AppButton variant="secondary" @click="copyBackup">{{ copied ? 'Copied' : 'Copy codes' }}</AppButton>
        <AppButton variant="primary" @click="backupCodes = []">I saved them</AppButton>
      </div>
    </template>

    <template v-else-if="setup">
      <ol class="two-factor__steps">
        <li>Scan this code with an authenticator app (Google Authenticator, Aegis, 1Password, …).</li>
        <li>Type the six-digit code the app shows to finish.</li>
      </ol>
      <svg
        v-if="qr"
        class="two-factor__qr"
        :viewBox="`0 0 ${qr.size} ${qr.size}`"
        role="img"
        aria-label="Two-factor setup QR code"
        shape-rendering="crispEdges"
      >
        <rect width="100%" height="100%" fill="#fff" />
        <path :d="qr.path" fill="#000" />
      </svg>
      <p>Can’t scan? Enter this key by hand: <code>{{ setup.secret }}</code></p>
      <div class="two-factor__actions">
        <input
          v-model="code"
          class="two-factor__input"
          inputmode="numeric"
          autocomplete="one-time-code"
          maxlength="7"
          placeholder="123456"
          aria-label="Six-digit code"
          @keyup.enter="confirmSetup"
        />
        <AppButton variant="primary" :busy="busy" :disabled="!code.trim()" @click="confirmSetup">
          Turn on
        </AppButton>
        <AppButton variant="secondary" @click="cancelSetup">Cancel</AppButton>
      </div>
    </template>

    <template v-else-if="status && !status.enabled">
      <div class="two-factor__actions">
        <AppButton variant="primary" :busy="busy" @click="startSetup">Set up two-factor</AppButton>
      </div>
    </template>

    <template v-else-if="status && status.enabled">
      <p>{{ status.backup_codes_remaining }} backup code(s) left.</p>
      <div v-if="!confirmingOff" class="two-factor__actions">
        <AppButton variant="danger" @click="confirmingOff = true">Turn off…</AppButton>
      </div>
      <div v-else class="two-factor__actions">
        <input
          v-model="code"
          class="two-factor__input"
          inputmode="text"
          autocomplete="one-time-code"
          maxlength="12"
          placeholder="Code or backup code"
          aria-label="Current code or backup code"
          @keyup.enter="turnOff"
        />
        <AppButton variant="danger" :busy="busy" :disabled="!code.trim()" @click="turnOff">
          Turn off
        </AppButton>
        <AppButton variant="secondary" @click="confirmingOff = false">Cancel</AppButton>
      </div>
      <p class="two-factor__hint">
        Lost your phone and backup codes? On the host, run <code>vibepollo --reset-2fa</code> to turn
        this off, then set it up again.
      </p>
    </template>
  </section>
</template>

<style scoped>
.two-factor {
  margin-bottom: var(--vs-space-24);
  padding: var(--vs-space-16) var(--vs-space-20);
  border: 1px solid var(--vs-color-border-subtle);
  border-radius: var(--vs-radius-card);
  background: var(--vs-color-bg-surface);
  font-size: var(--vs-type-size-control);
}
.two-factor__heading {
  display: flex;
  align-items: center;
  gap: var(--vs-space-12);
}
h3 {
  margin: 0;
  font-size: var(--vs-type-size-control);
}
p {
  margin: var(--vs-space-12) 0 0;
}
.two-factor__steps {
  margin: var(--vs-space-12) 0 0;
  padding-left: var(--vs-space-20);
}
.two-factor__qr {
  display: block;
  width: 220px;
  height: 220px;
  margin: var(--vs-space-12) 0;
  border-radius: var(--vs-radius-card);
}
.two-factor__codes {
  margin: var(--vs-space-12) 0;
  padding: var(--vs-space-12);
  border-radius: var(--vs-radius-card);
  background: var(--vs-color-bg-subtle);
  font-size: 1.05em;
}
.two-factor__actions {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: var(--vs-space-12);
  margin-top: var(--vs-space-16);
}
.two-factor__input {
  min-width: 11rem;
  padding: var(--vs-space-8) var(--vs-space-12);
  border: 1px solid var(--vs-color-border-subtle);
  border-radius: var(--vs-radius-card);
  background: var(--vs-color-bg-surface);
  color: inherit;
  font: inherit;
  letter-spacing: 0.08em;
}
.two-factor__hint {
  color: var(--vs-color-text-secondary);
}
</style>
