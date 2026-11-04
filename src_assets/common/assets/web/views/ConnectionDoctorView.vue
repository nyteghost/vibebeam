<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import { useI18n } from 'vue-i18n';
import { apiGet } from '@/api/client';
import { AppButton, PageHeader, StatusBadge, type StatusTone } from '@/components/ui';
import { buildDoctorReport, doctorSupportSummary, type DoctorReport, type DoctorState } from '@/utils/connectionDoctor';

const { t, locale } = useI18n();
const report = ref<DoctorReport | null>(null);
const running = ref(false);
const copyState = ref<'idle' | 'copied' | 'manual'>('idle');
const summaryArea = ref<HTMLTextAreaElement | null>(null);
let controller: AbortController | null = null;
let disposed = false;
const tones: Record<DoctorState, StatusTone> = { reported: 'success', attention: 'warning', unknown: 'neutral', info: 'info' };
const summary = computed(() => report.value ? doctorSupportSummary(report.value) : '');
const needsAttention = computed(() => report.value?.checks.filter((check) => check.state === 'attention').length ?? 0);
const unknown = computed(() => report.value?.checks.filter((check) => check.state === 'unknown').length ?? 0);
const checkedAt = computed(() => report.value ? new Intl.DateTimeFormat(locale.value, {
  hour: 'numeric', minute: '2-digit', second: '2-digit',
}).format(report.value.capturedAt) : '');
async function runChecks(): Promise<void> {
  if (running.value || disposed) return;
  running.value = true;
  report.value = null; // A new request cannot leave an older green result on display.
  copyState.value = 'idle';
  const current = new AbortController();
  controller = current;
  const timeout = window.setTimeout(() => current.abort(), 8000);
  try {
    const results = await Promise.allSettled([
      apiGet<unknown>('/api/metadata', { signal: current.signal, cache: 'no-store' }),
      apiGet<unknown>('/api/session/status', { signal: current.signal, cache: 'no-store' }),
    ]);
    if (disposed || controller !== current) return;
    report.value = buildDoctorReport(
      results[0].status === 'fulfilled' ? results[0].value : null,
      results[1].status === 'fulfilled' ? results[1].value : null,
      Date.now(),
    );
  } finally {
    window.clearTimeout(timeout);
    if (controller === current) { controller = null; running.value = false; }
  }
}
async function copySummary(): Promise<void> {
  if (!summary.value) return;
  try {
    await navigator.clipboard.writeText(summary.value);
    if (!disposed) copyState.value = 'copied';
  } catch {
    if (!disposed) { copyState.value = 'manual'; summaryArea.value?.focus(); summaryArea.value?.select(); }
  }
}
onMounted(() => { void runChecks(); });
onBeforeUnmount(() => { disposed = true; controller?.abort(); controller = null; });
</script>

<template>
  <div class="page page--wide doctor-page">
    <PageHeader :title="t('ui.doctor.title')" :description="t('ui.doctor.description')">
      <template #actions>
        <AppButton :label="t('ui.doctor.run')" :busy="running" :busy-label="t('ui.doctor.running')" icon="refresh" @click="runChecks" />
      </template>
    </PageHeader>
    <p class="doctor-scope">{{ t('ui.doctor.scope') }}</p>
    <p v-if="running" role="status">{{ t('ui.doctor.running') }}</p>
    <template v-if="report">
      <p role="status" aria-live="polite">{{ t('ui.doctor.result', { attention: needsAttention, unknown, time: checkedAt }) }}</p>
      <ul class="doctor-checks">
        <li v-for="check in report.checks" :key="check.id" class="panel doctor-check" :data-check="check.id" :data-state="check.state">
          <div class="doctor-check__heading">
            <h2>{{ t(`ui.doctor.checks.${check.id}`) }}</h2>
            <StatusBadge :label="t(`ui.doctor.states.${check.state}`)" :tone="tones[check.state]" />
          </div>
          <p>{{ t(`ui.doctor.findings.${check.detail}`, check.params ?? {}) }}</p>
          <RouterLink :to="check.action">{{ t(`ui.doctor.actions.${check.id}`) }}</RouterLink>
        </li>
      </ul>
      <section class="panel doctor-summary" aria-labelledby="doctor-summary-title">
        <h2 id="doctor-summary-title">{{ t('ui.doctor.summaryTitle') }}</h2>
        <p>{{ t('ui.doctor.summaryPrivacy') }}</p>
        <label for="doctor-summary">{{ t('ui.doctor.summaryPreview') }}</label>
        <textarea id="doctor-summary" ref="summaryArea" readonly :value="summary" rows="8" spellcheck="false" />
        <AppButton :label="t('ui.doctor.copy')" @click="copySummary" />
        <p v-if="copyState !== 'idle'" role="status">{{ t(`ui.doctor.${copyState}`) }}</p>
      </section>
    </template>
  </div>
</template>

<style scoped>
.doctor-page { display: grid; gap: var(--vs-space-20); }
.doctor-scope { color: var(--vs-color-text-secondary); margin: 0; }
.doctor-checks { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 22rem), 1fr)); gap: var(--vs-space-16); padding: 0; margin: 0; list-style: none; }
.doctor-check, .doctor-summary { padding: var(--vs-space-20); min-inline-size: 0; }
.doctor-check__heading { display: flex; flex-wrap: wrap; gap: var(--vs-space-12); align-items: center; justify-content: space-between; }
.doctor-check h2, .doctor-summary h2 { margin: 0; font-size: var(--vs-type-size-control); }
.doctor-check p { line-height: 1.6; color: var(--vs-color-text-secondary); }
.doctor-summary { display: grid; gap: var(--vs-space-12); }
.doctor-summary textarea { inline-size: 100%; min-inline-size: 0; box-sizing: border-box; resize: vertical; font-family: monospace; background: var(--vs-color-bg-canvas); color: var(--vs-color-text-primary); border: 1px solid var(--vs-color-border-subtle); border-radius: var(--vs-radius-card); padding: var(--vs-space-12); }
.doctor-summary .vs-button { justify-self: start; }
</style>
