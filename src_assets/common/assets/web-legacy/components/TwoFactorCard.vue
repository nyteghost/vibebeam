<template>
  <n-card class="mb-0" :segmented="{ content: true, footer: false }">
    <template #header>
      <div class="flex items-center gap-3">
        <span class="text-lg"><i class="fas fa-key" /></span>
        <h2 class="text-lg font-medium">Require 2FA to connect</h2>
        <n-tag v-if="status" size="small" :type="status.enabled ? 'success' : 'default'" round>
          {{ status.enabled ? 'On' : 'Off' }}
        </n-tag>
      </div>
    </template>

    <p class="text-sm opacity-80 mb-3">
      When this is on, Vibelight asks for a six-digit code from your authenticator app every time
      someone starts or resumes a stream. Apps that cannot type a code (for example the stock iOS
      Moonlight app) need “Exempt from 2FA” turned on for that client below.
    </p>

    <div v-if="error" class="text-sm mb-3" style="color: #e5484d">{{ error }}</div>

    <template v-if="backupCodes.length">
      <p class="font-medium">Save your backup codes now</p>
      <p class="text-sm opacity-80">Each code works once if you lose your phone. They will not be shown again.</p>
      <pre class="my-3 p-3 rounded" style="background: rgba(127, 127, 127, 0.15)">{{
        backupCodes.join('\n')
      }}</pre>
      <div class="flex gap-2">
        <n-button secondary @click="copyBackup">{{ copied ? 'Copied' : 'Copy codes' }}</n-button>
        <n-button type="primary" @click="backupCodes = []">I saved them</n-button>
      </div>
    </template>

    <template v-else-if="setup">
      <ol class="list-decimal pl-5 text-sm mb-3">
        <li>Scan this code with an authenticator app (Google Authenticator, Aegis, 1Password, …).</li>
        <li>Type the six-digit code the app shows to finish.</li>
      </ol>
      <svg
        v-if="qr"
        :viewBox="`0 0 ${qr.size} ${qr.size}`"
        role="img"
        aria-label="Two-factor setup QR code"
        shape-rendering="crispEdges"
        style="width: 220px; height: 220px; display: block; margin-bottom: 12px"
      >
        <rect width="100%" height="100%" fill="#fff" />
        <path :d="qr.path" fill="#000" />
      </svg>
      <p class="text-sm mb-3">
        Can’t scan? Enter this key by hand: <code>{{ setup.secret }}</code>
      </p>
      <div class="flex flex-wrap items-center gap-2">
        <n-input
          v-model:value="code"
          style="max-width: 12rem"
          placeholder="123456"
          :maxlength="7"
          @keyup.enter="confirmSetup"
        />
        <n-button type="primary" :loading="busy" :disabled="!code.trim()" @click="confirmSetup">
          Turn on
        </n-button>
        <n-button secondary @click="cancelSetup">Cancel</n-button>
      </div>
    </template>

    <template v-else-if="status && !status.enabled">
      <n-button type="primary" :loading="busy" @click="startSetup">Set up two-factor</n-button>
    </template>

    <template v-else-if="status && status.enabled">
      <p class="text-sm mb-3">{{ status.backup_codes_remaining }} backup code(s) left.</p>
      <n-button v-if="!confirmingOff" type="error" secondary @click="confirmingOff = true">
        Turn off…
      </n-button>
      <div v-else class="flex flex-wrap items-center gap-2">
        <n-input
          v-model:value="code"
          style="max-width: 14rem"
          placeholder="Code or backup code"
          :maxlength="12"
          @keyup.enter="turnOff"
        />
        <n-button type="error" :loading="busy" :disabled="!code.trim()" @click="turnOff">
          Turn off
        </n-button>
        <n-button secondary @click="confirmingOff = false">Cancel</n-button>
      </div>
      <p class="text-xs opacity-70 mt-3">
        Lost your phone and backup codes? On the host, run <code>vibepollo --reset-2fa</code> to turn
        this off, then set it up again.
      </p>
    </template>
  </n-card>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { NButton, NCard, NInput, NTag } from 'naive-ui';
import { http } from '@/http';
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

function failure(data: unknown, fallback: string): string {
  const message = (data as { error?: unknown } | undefined)?.error;
  return typeof message === 'string' && message ? message : fallback;
}

async function load() {
  const r = await http.get<TwoFactorStatus>('./api/two-factor', { validateStatus: () => true });
  if (r.status >= 200 && r.status < 300) {
    status.value = r.data;
  } else {
    error.value = 'Could not read the two-factor status from the host.';
  }
}

async function startSetup() {
  busy.value = true;
  error.value = '';
  code.value = '';
  const r = await http.post<SetupResponse>('./api/two-factor/setup', {}, { validateStatus: () => true });
  busy.value = false;
  if (r.status >= 200 && r.status < 300) {
    setup.value = r.data;
  } else {
    error.value = failure(r.data, 'Could not start the setup.');
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
  const r = await http.post<{ backup_codes: string[] }>(
    './api/two-factor/enable',
    { code: code.value },
    { validateStatus: () => true },
  );
  busy.value = false;
  if (r.status >= 200 && r.status < 300) {
    backupCodes.value = r.data.backup_codes;
    setup.value = null;
    code.value = '';
    await load();
  } else {
    error.value = failure(r.data, 'That code was not accepted.');
  }
}

async function turnOff() {
  busy.value = true;
  error.value = '';
  const r = await http.post('./api/two-factor/disable', { code: code.value }, { validateStatus: () => true });
  busy.value = false;
  if (r.status >= 200 && r.status < 300) {
    code.value = '';
    confirmingOff.value = false;
    await load();
  } else {
    error.value = failure(r.data, 'That code was not accepted.');
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
