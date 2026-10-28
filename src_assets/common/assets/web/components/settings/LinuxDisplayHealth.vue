<script setup lang="ts">
import { onMounted, ref } from 'vue';
import { apiGet } from '@/api/client';
import { AppButton, StatusBadge } from '@/components/ui';

interface HealthCheck {
  id: string;
  ok: boolean;
  detail: string;
}
interface DisplayHealth {
  healthy: boolean;
  summary: string;
  checks: HealthCheck[];
  fix_commands: string[];
}

const health = ref<DisplayHealth | null>(null);
const loading = ref(true);
const failed = ref(false);
const copied = ref(false);

const labels: Record<string, string> = {
  kernel: 'Kernel',
  reboot: 'Update finished',
  dkms: 'Module build',
  module: 'Module loaded',
  connectors: 'Virtual displays',
};

async function load() {
  loading.value = true;
  failed.value = false;
  try {
    health.value = await apiGet<DisplayHealth>('/api/linux/display-health');
  } catch {
    failed.value = true;
  } finally {
    loading.value = false;
  }
}

async function copyFix() {
  if (!health.value) return;
  try {
    await navigator.clipboard.writeText(health.value.fix_commands.join('\n'));
    copied.value = true;
    window.setTimeout(() => (copied.value = false), 2500);
  } catch {
    copied.value = false;
  }
}

onMounted(load);
</script>

<template>
  <section class="display-health" aria-labelledby="display-health-title">
    <header class="display-health__heading">
      <h3 id="display-health-title">Display health</h3>
      <StatusBadge
        v-if="health"
        :label="health.healthy ? 'Ready' : 'Needs attention'"
        :tone="health.healthy ? 'success' : 'warning'"
      />
    </header>
    <p v-if="loading">Checking the virtual display…</p>
    <p v-else-if="failed">Could not read the display health from the host.</p>
    <template v-else-if="health">
      <p>{{ health.summary }}</p>
      <ul class="display-health__checks">
        <li v-for="item in health.checks" :key="item.id">
          <StatusBadge :label="item.ok ? 'OK' : 'Problem'" :tone="item.ok ? 'success' : 'warning'" />
          <strong>{{ labels[item.id] || item.id }}</strong>
          <span>{{ item.detail }}</span>
        </li>
      </ul>
      <div v-if="health.fix_commands.length" class="display-health__fix">
        <p>Run these on the host, in order:</p>
        <pre>{{ health.fix_commands.join('\n') }}</pre>
        <AppButton variant="secondary" @click="copyFix">{{
          copied ? 'Copied' : 'Copy commands'
        }}</AppButton>
      </div>
    </template>
    <div class="display-health__actions">
      <AppButton variant="secondary" @click="load">Check again</AppButton>
      <a class="button button--secondary" href="/api/linux/diagnostics" download>Download diagnostics</a>
    </div>
  </section>
</template>

<style scoped>
.display-health {
  margin-bottom: var(--vs-space-24);
  padding: var(--vs-space-16) var(--vs-space-20);
  border: 1px solid var(--vs-color-border-subtle);
  border-radius: var(--vs-radius-card);
  background: var(--vs-color-bg-surface);
  font-size: var(--vs-type-size-control);
}
.display-health__heading {
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
.display-health__checks {
  margin: var(--vs-space-12) 0 0;
  padding: 0;
  list-style: none;
  display: grid;
  gap: var(--vs-space-8);
}
.display-health__checks li {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: var(--vs-space-8);
}
.display-health__checks span {
  color: var(--vs-color-text-secondary);
}
pre {
  margin: var(--vs-space-8) 0;
  padding: var(--vs-space-12);
  overflow-x: auto;
  border-radius: var(--vs-radius-card);
  background: var(--vs-color-bg-subtle);
}
.display-health__actions {
  display: flex;
  flex-wrap: wrap;
  gap: var(--vs-space-12);
  margin-top: var(--vs-space-16);
}
</style>
