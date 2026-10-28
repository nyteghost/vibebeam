<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { RouterLink } from 'vue-router';
import { apiGet } from '@/api/client';
import { AppButton } from '@/components/ui';

const storageKey = 'vibebeam.first-run-checklist.dismissed';

function readDismissed(): boolean {
  try {
    return localStorage.getItem(storageKey) === '1';
  } catch {
    return false;
  }
}

const dismissed = ref(readDismissed());
const pairedDevices = ref<number | null>(null);
const twoFactorOn = ref<boolean | null>(null);

const steps = computed(() => [
  {
    key: 'pair',
    done: pairedDevices.value === null ? null : pairedDevices.value > 0,
    title: 'Pair a device',
    detail: 'Open Vibelight or Moonlight, choose this PC and enter the PIN it shows.',
    to: '/pair',
    action: 'Pair now',
  },
  {
    key: '2fa',
    done: twoFactorOn.value,
    title: 'Turn on two-factor sign-in (recommended)',
    detail:
      'Asks for a code from an authenticator app when someone starts a stream. Save the backup codes.',
    to: '/devices',
    action: 'Set up 2FA',
  },
  {
    key: 'recovery',
    done: null,
    title: 'Know the screen recovery button',
    detail:
      'If your PC screen stays blank after streaming, open this page from another device and press Terminate virtual screen.',
    to: '/',
    action: '',
  },
  {
    key: 'scope',
    done: null,
    title: 'Check who can reach this web console',
    detail: 'Keep it to your own network unless you need it from elsewhere.',
    to: '/settings',
    action: 'Open settings',
  },
]);

function dismiss(): void {
  dismissed.value = true;
  try {
    localStorage.setItem(storageKey, '1');
  } catch {
    /* the card simply comes back next time */
  }
}

onMounted(async () => {
  if (dismissed.value) return;
  const [clients, twoFactor] = await Promise.allSettled([
    apiGet<{ named_certs?: unknown[] }>('/api/clients/list'),
    apiGet<{ enabled?: boolean }>('/api/two-factor'),
  ]);
  if (clients.status === 'fulfilled') {
    pairedDevices.value = Array.isArray(clients.value.named_certs)
      ? clients.value.named_certs.length
      : 0;
  }
  if (twoFactor.status === 'fulfilled') twoFactorOn.value = twoFactor.value.enabled === true;
});
</script>

<template>
  <section v-if="!dismissed" class="first-run" aria-labelledby="first-run-title">
    <div class="first-run__heading">
      <h2 id="first-run-title">Getting started</h2>
      <AppButton variant="secondary" @click="dismiss">Hide this</AppButton>
    </div>
    <ul class="first-run__list">
      <li v-for="step in steps" :key="step.key">
        <span
          class="first-run__mark"
          :class="{ 'first-run__mark--done': step.done === true }"
          aria-hidden="true"
          >{{ step.done === true ? '✓' : '•' }}</span
        >
        <div>
          <strong>{{ step.title }}</strong>
          <span v-if="step.done === true" class="first-run__done"> (done)</span>
          <p>{{ step.detail }}</p>
          <RouterLink v-if="step.action && step.done !== true" :to="step.to">{{
            step.action
          }}</RouterLink>
        </div>
      </li>
    </ul>
  </section>
</template>

<style scoped>
.first-run {
  padding: var(--vs-space-16) var(--vs-space-20);
  border: 1px solid var(--vs-color-border-subtle);
  border-radius: var(--vs-radius-card);
  background: var(--vs-color-bg-surface);
  font-size: var(--vs-type-size-control);
}
.first-run__heading {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--vs-space-12);
}
h2 {
  margin: 0;
  font-size: var(--vs-type-size-control);
}
.first-run__list {
  margin: var(--vs-space-12) 0 0;
  padding: 0;
  list-style: none;
  display: grid;
  gap: var(--vs-space-12);
}
.first-run__list li {
  display: flex;
  gap: var(--vs-space-12);
}
.first-run__list p {
  margin: var(--vs-space-4, 4px) 0;
  opacity: 0.8;
}
.first-run__mark {
  flex: none;
  width: 1.5em;
  text-align: center;
  opacity: 0.6;
}
.first-run__mark--done {
  opacity: 1;
  color: var(--vs-color-success, #3a9d5d);
}
.first-run__done {
  opacity: 0.7;
}
</style>
