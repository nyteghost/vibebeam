<script setup lang="ts">
import { onMounted, ref } from 'vue';
import { apiGet } from '@/api/client';
import { AppButton } from '@/components/ui';

interface ReceivedFile {
  name: string;
  size: number;
  modified: number;
}
interface ReceivedFiles {
  path: string;
  total: number;
  files: ReceivedFile[];
}

const data = ref<ReceivedFiles | null>(null);
const failed = ref(false);
const copied = ref(false);

function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(0)} KB`;
  return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
}

function formatTime(seconds: number): string {
  return new Date(seconds * 1000).toLocaleString();
}

async function load(): Promise<void> {
  try {
    data.value = await apiGet<ReceivedFiles>('/api/files/received');
    failed.value = false;
  } catch {
    failed.value = true;
  }
}

async function copyPath(): Promise<void> {
  if (!data.value) return;
  try {
    await navigator.clipboard.writeText(data.value.path);
    copied.value = true;
    setTimeout(() => (copied.value = false), 1500);
  } catch {
    /* clipboard may be unavailable on plain http; the path is still shown */
  }
}

onMounted(load);
</script>

<template>
  <section class="received-files" aria-labelledby="received-files-title">
    <div class="received-files__heading">
      <h3 id="received-files-title">Received files</h3>
      <AppButton variant="secondary" @click="load">Refresh</AppButton>
    </div>
    <p v-if="failed">Could not read the receive folder from the host.</p>
    <template v-else-if="data">
      <p>
        Files sent from Vibelight are saved on this computer in
        <code>{{ data.path }}</code>
        <AppButton variant="secondary" @click="copyPath">{{ copied ? 'Copied' : 'Copy path' }}</AppButton>
      </p>
      <p v-if="!data.total">Nothing received yet.</p>
      <ul v-else class="received-files__list">
        <li v-for="file in data.files" :key="file.name + file.modified">
          <span class="received-files__name">{{ file.name }}</span>
          <span class="received-files__meta">{{ formatSize(file.size) }} · {{ formatTime(file.modified) }}</span>
        </li>
      </ul>
      <p v-if="data.total > data.files.length" class="received-files__meta">
        Showing the newest {{ data.files.length }} of {{ data.total }}.
      </p>
    </template>
  </section>
</template>

<style scoped>
.received-files {
  margin-bottom: var(--vs-space-24);
  padding: var(--vs-space-16) var(--vs-space-20);
  border: 1px solid var(--vs-color-border-subtle);
  border-radius: var(--vs-radius-card);
  background: var(--vs-color-bg-surface);
  font-size: var(--vs-type-size-control);
}
.received-files__heading {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--vs-space-12);
}
h3 {
  margin: 0;
  font-size: var(--vs-type-size-control);
}
p {
  margin: var(--vs-space-12) 0 0;
}
code {
  overflow-wrap: anywhere;
}
.received-files__list {
  margin: var(--vs-space-12) 0 0;
  padding: 0;
  list-style: none;
}
.received-files__list li {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  gap: var(--vs-space-8);
  padding: var(--vs-space-8) 0;
  border-top: 1px solid var(--vs-color-border-subtle);
}
.received-files__name {
  overflow-wrap: anywhere;
}
.received-files__meta {
  opacity: 0.7;
}
</style>
