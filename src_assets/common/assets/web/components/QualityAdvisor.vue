<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { apiGet } from '@/api/client';
import { recommendQuality, type IntelligenceMode } from '@/utils/vibeIntelligence';
import { QualityTelemetry, type SessionQuality } from '@/utils/qualityTelemetry';
import { readIntelligencePreferences, writeIntelligencePreferences, type IntelligencePreferences } from '@/utils/intelligencePreferences';

function loadPreferences(): IntelligencePreferences {
  try { return readIntelligencePreferences(window.localStorage); }
  catch { return { mode: 'smart-local', dismissed: [] }; }
}
const preferences = ref(loadPreferences());
const mode = computed<IntelligenceMode>({
  get: () => preferences.value.mode,
  set: (value) => { preferences.value.mode = value; },
});
const saved = ref(true);
watch(preferences, (value) => {
  try { saved.value = writeIntelligencePreferences(window.localStorage, value); }
  catch { saved.value = false; }
}, { deep: true });

const telemetry = new QualityTelemetry();
const sessions = ref<SessionQuality[]>([]);
const selected = ref('');
const selectedSession = computed(() => sessions.value.find((session) => session.key === selected.value));
const samples = computed(() => selectedSession.value?.samples ?? []);
const latest = computed(() => samples.value[samples.value.length - 1]);
const advice = computed(() => recommendQuality(mode.value, samples.value, Date.now())
  .filter((item) => !preferences.value.dismissed.includes(item.id)));
const dismissedForVisit = ref(false);
const unavailable = ref(false);
let timer: number | undefined;
let request: AbortController | null = null;
let disposed = false;

function reset(): void {
  request?.abort();
  request = null;
  telemetry.clear();
  sessions.value = [];
  selected.value = '';
  unavailable.value = false;
  dismissedForVisit.value = false;
}
async function refresh(): Promise<void> {
  if (disposed || request || mode.value === 'off' || document.visibilityState !== 'visible') return;
  const controller = new AbortController();
  request = controller;
  const timeout = window.setTimeout(() => controller.abort(), 8000);
  try {
    const payload = await apiGet<unknown>('/api/history/sessions/active', { signal: controller.signal });
    if (disposed || controller.signal.aborted || request !== controller) return;
    const data = payload as { sessions?: unknown; status?: unknown } | null;
    if (!data || !Array.isArray(data.sessions) || data.status === false) throw new Error('unsupported telemetry');
    sessions.value = telemetry.observe(payload, Date.now());
    if (!sessions.value.some((session) => session.key === selected.value)) selected.value = sessions.value[0]?.key ?? '';
    unavailable.value = false;
  } catch {
    if (!disposed && request === controller) {
      telemetry.clear();
      sessions.value = [];
      selected.value = '';
      unavailable.value = true;
    }
  } finally {
    window.clearTimeout(timeout);
    if (request === controller) request = null;
  }
}
watch(mode, () => { reset(); void refresh(); });
watch(selected, () => { dismissedForVisit.value = false; });
function dismissTypes(): void {
  preferences.value.dismissed = [...new Set([...preferences.value.dismissed, ...advice.value.map((item) => item.id)])];
}
function visibilityChanged(): void {
  // A suspended page cannot establish a continuous measurement window.
  reset();
  void refresh();
}
onMounted(() => {
  void refresh();
  timer = window.setInterval(() => { void refresh(); }, 5000);
  document.addEventListener('visibilitychange', visibilityChanged);
});
onBeforeUnmount(() => {
  disposed = true;
  reset();
  if (timer) window.clearInterval(timer);
  document.removeEventListener('visibilitychange', visibilityChanged);
});
</script>

<template>
  <section class="panel intelligence-panel" aria-labelledby="intelligence-title">
    <div class="panel__heading">
      <div>
        <h2 id="intelligence-title">Vibe Intelligence <span class="intelligence-panel__beta">Beta</span></h2>
        <p>Optional, private and offline-first quality guidance. Nothing is changed automatically.</p>
      </div>
      <label class="intelligence-panel__mode">
        <span>Mode</span>
        <select v-model="mode" aria-label="Vibe Intelligence mode" class="vs-select">
          <option value="off">Off</option>
          <option value="smart-local">Smart Local</option>
          <option value="local-ai" disabled>Local AI (not available yet)</option>
        </select>
      </label>
    </div>
    <p class="intelligence-panel__note">Choices are saved in this browser for this host address. Smart Local uses rules without a model.</p>
    <p v-if="!saved" role="status">Browser storage is unavailable. Your choices apply until this page is closed.</p>
    <button v-if="preferences.dismissed.length" type="button" class="vs-button"
      @click="preferences.dismissed = []; dismissedForVisit = false">Restore dismissed suggestions</button>
    <p v-if="mode === 'off'">Recommendations are turned off.</p>
    <template v-else>
      <p v-if="unavailable" class="intelligence-panel__note">Stream measurements unavailable. Existing streaming controls continue to work.</p>
      <p v-else-if="!sessions.length" class="intelligence-panel__note">No active stream measurements available yet.</p>
      <template v-else>
        <label class="intelligence-panel__mode">
          <span>Session</span>
          <select v-model="selected" class="vs-select" aria-label="Quality guidance session">
            <option v-for="session in sessions" :key="session.key" :value="session.key">{{ session.label }}</option>
          </select>
        </label>
        <p v-if="latest" class="intelligence-panel__note">Host output: {{ latest.fps?.toFixed(1) }} / {{ latest.targetFps }} FPS · {{ samples.length }} observations</p>
        <p class="intelligence-panel__note">Measures host output only. Client FPS, network latency and packet loss are unavailable here. Static content can produce fewer frames.</p>
        <p v-if="samples.length < 6" class="intelligence-panel__note">Measuring this session. Guidance needs at least six continuous observations over 15 seconds.</p>
        <p v-else-if="dismissedForVisit" class="intelligence-panel__note">Suggestions hidden until you leave this page or choose another session.</p>
        <div v-else-if="advice.length" role="status">
          <div v-for="item in advice" :key="item.id">
            <strong>{{ item.summary }}</strong>
            <p>{{ item.evidence }} {{ item.suggestion }}</p>
          </div>
          <p>Changes require manual confirmation; automatic Apply and Undo are not enabled.</p>
          <button type="button" class="vs-button" @click="dismissedForVisit = true">Not now</button>
          <button type="button" class="vs-button" @click="dismissTypes">Don't suggest these again for this host</button>
        </div>
        <p v-else class="intelligence-panel__note">No sustained issue detected in the available host FPS measurements. Dismissed suggestion types remain hidden.</p>
      </template>
    </template>
  </section>
</template>

<style scoped>
.intelligence-panel { display: grid; gap: var(--vs-space-12); min-inline-size: 0; }
.panel__heading { display: flex; flex-wrap: wrap; justify-content: space-between; gap: var(--vs-space-16); }
.panel__heading > div { flex: 1 1 20rem; min-inline-size: 0; }
.intelligence-panel__mode { max-inline-size: 100%; }
.vs-select { inline-size: 100%; min-inline-size: 0; max-inline-size: 100%; }
.vs-button { justify-self: start; white-space: normal; }
</style>
