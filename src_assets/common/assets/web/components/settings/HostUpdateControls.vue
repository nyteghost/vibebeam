<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from 'vue';
import { apiGet } from '@/api/client';
import { AppButton, ConfirmDialog, InlineAlert } from '@/components/ui';
import { useSystemStore } from '@/stores/system';
import {
  canLaunchHostUpdate,
  hostManifestFromRelease,
  hostUpdateUri,
  selectHostRelease,
  verifyHostManifest,
  type HostRelease,
  type HostUpdate,
  type HostUpdateChannel,
} from '@/utils/hostUpdates';

const system = useSystemStore();
function savedChannel(): HostUpdateChannel {
  try {
    return localStorage.getItem('vibebeam.update-channel') === 'main' ? 'main' : 'beta';
  } catch {
    return 'beta';
  }
}
const channel = ref<HostUpdateChannel>(savedChannel());
const candidate = ref<HostUpdate | null>(null);
const checked = ref(false);
const busy = ref(false);
const error = ref('');
const notice = ref('');
const confirm = ref(false);
let request: AbortController | undefined;
let generation = 0;
const local = computed(() =>
  canLaunchHostUpdate(
    window.location.hostname,
    system.metadata?.platform,
    system.metadata?.architecture,
  ),
);
const current = computed(() => candidate.value?.commit === system.metadata?.commit);
function changeChannel() {
  generation++;
  request?.abort();
  busy.value = false;
  candidate.value = null;
  checked.value = false;
  error.value = '';
  notice.value = '';
  confirm.value = false;
  try {
    localStorage.setItem('vibebeam.update-channel', channel.value);
  } catch {
    /* Usable without storage. */
  }
}
async function check() {
  if (busy.value) return;
  const token = ++generation;
  const selected = channel.value;
  const controller = new AbortController();
  request = controller;
  const timer = setTimeout(() => controller.abort(), 15000);
  busy.value = true;
  error.value = '';
  notice.value = '';
  candidate.value = null;
  checked.value = false;
  try {
    const releases: HostRelease[] = [];
    for (let page = 1; page <= 3; page++) {
      const response = await fetch(
        `https://api.github.com/repos/nyteghost/vibebeam/releases?per_page=100&page=${page}`,
        { credentials: 'omit', signal: controller.signal },
      );
      if (!response.ok) throw new Error('Release check failed');
      const batch = await response.json();
      if (!Array.isArray(batch)) throw new Error('Invalid releases');
      releases.push(...batch);
      if (batch.length < 100) break;
    }
    const release = selectHostRelease(releases, selected);
    let update: HostUpdate | null = null;
    if (release) update = verifyHostManifest(release, hostManifestFromRelease(release), selected);
    if (token !== generation) return;
    candidate.value = update;
    checked.value = true;
  } catch {
    if (token === generation)
      error.value =
        'Could not verify this channel’s update. Retry the check; nothing was installed.';
  } finally {
    clearTimeout(timer);
    if (token === generation) busy.value = false;
  }
}
async function launch() {
  confirm.value = false;
  if (!local.value || !candidate.value || current.value || busy.value) return;
  const token = ++generation;
  const selected = channel.value;
  busy.value = true;
  error.value = '';
  try {
    // Refresh authenticated session evidence immediately before the desktop handoff.
    const state = await apiGet<{ status: boolean; activeSessions: number; appRunning: boolean }>(
      '/api/session/status',
      { signal: AbortSignal.timeout(5000) },
    );
    if (token !== generation) return;
    if (
      state.status !== true ||
      !Number.isInteger(state.activeSessions) ||
      state.activeSessions < 0 ||
      typeof state.appRunning !== 'boolean'
    )
      throw new Error('Unknown session state');
    if (state.activeSessions > 0 || state.appRunning) {
      error.value =
        'Disconnect streaming clients and close the managed application before updating.';
      return;
    }
    window.location.assign(hostUpdateUri(selected));
    notice.value =
      'Installer requested. Approve the browser’s Open Vibebeam Update prompt, then follow the native password and installation prompts. If no window opens, install the desktop updater package first.';
  } catch {
    if (token !== generation) return;
    error.value =
      'Could not confirm that the host is idle or open the installer. Nothing was installed by this page.';
  } finally {
    if (token === generation) busy.value = false;
  }
}
onBeforeUnmount(() => {
  generation++;
  request?.abort();
});
</script>
<template>
  <section class="host-update-controls" aria-labelledby="host-updates-title">
    <h2 id="host-updates-title">Host updates</h2>
    <p>
      Choose your update channel. Download, verification, backup and installation run in the native
      installer after approval.
    </p>
    <div class="host-update-controls__actions">
      <label for="host-update-channel">Update channel</label>
      <select id="host-update-channel" v-model="channel" :disabled="busy" @change="changeChannel">
        <option value="main">Main</option>
        <option value="beta">Beta</option>
      </select>
      <AppButton
        label="Check for update"
        :busy="busy"
        busy-label="Checking…"
        variant="secondary"
        @click="check"
      />
      <AppButton
        label="Update"
        :disabled="!candidate || current || !local || busy || system.isStreaming"
        @click="confirm = true"
      />
    </div>
    <p v-if="candidate">
      {{ current ? 'Already installed' : 'Available' }}:
      {{ channel === 'beta' ? 'Beta' : 'Main' }} · {{ candidate.commit.slice(0, 8) }} ({{
        candidate.version
      }})
    </p>
    <p v-else-if="checked">
      No verified {{ channel === 'beta' ? 'Beta' : 'Main' }} package is published yet. The selected
      channel will not fall back to another channel.
    </p>
    <p v-if="system.isStreaming">
      Updates are paused while a stream or managed application is active.
    </p>
    <p v-if="!local">
      Installation is available on the CachyOS/Arch host at https://127.0.0.1:47990/v2/maintenance.
      A remote browser can check availability.
    </p>
    <InlineAlert v-if="error" tone="warning" :title="error" />
    <InlineAlert v-if="notice" tone="info" :title="notice" />
    <ConfirmDialog
      v-model:open="confirm"
      title="Install host update?"
      :description="`Update from the ${channel === 'beta' ? 'Beta' : 'Main'} channel? Installation restarts Vibebeam. The native installer verifies the package and requires a rollback package and fresh backup.`"
      confirm-label="Open installer"
      @confirm="launch"
    />
  </section>
</template>
<style scoped>
.host-update-controls {
  display: grid;
  gap: var(--vs-space-12);
}
.host-update-controls h2,
.host-update-controls p {
  margin: 0;
}
.host-update-controls__actions {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: var(--vs-space-12);
}
select {
  padding: 8px 12px;
  background: var(--vs-color-bg-canvas);
  color: inherit;
  border: 1px solid currentColor;
  border-radius: 8px;
}
</style>
