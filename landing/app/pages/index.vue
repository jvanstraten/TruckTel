<script setup lang="ts">

import type { PluginInfo } from "~/types/info";
import AppDialog from "~/components/appDialog.vue";

const info = ref<PluginInfo | undefined>(undefined);
const error = ref<string | undefined>(undefined);

async function fetchInfo(): Promise<PluginInfo | string> {
  try {
    const response = await fetch("/api/info.json", { signal: AbortSignal.timeout(1000) });
    if (!response.ok) return "API did not respond";
    const data = await response.json();
    return data as PluginInfo;
  } catch (e) {
    return `${e}`;
  }
}

onMounted(async () => {
  document.title = "TruckTel landing";
  const result = await fetchInfo();
  if (typeof result == "string") {
    error.value = result;
  } else {
    info.value = result;
  }
});

</script>

<template>
  <v-container max-width="600">
    <v-card>
      <v-card-title class="text-center">Welcome to TruckTel!</v-card-title>
      <v-card-text v-if="info">
        <div class="opacity-50 pa-4 font-italic text-center">
          {{ info?.pluginVersion ?? "(unknown plugin)" }}
          running on
          {{ info?.gameVersion ?? "(unknown game)" }}
          <span v-if="info?.localIpAddress">
            at {{ info.localIpAddress }}<span v-if="info?.mdnsHostname">
              or, via mDNS, {{ info.mdnsHostname }}
            </span>.
          </span>
          <span v-else>
            on unknown IP address.
          </span>
        </div>
        <v-divider/>
        <v-list lines="two" select-strategy="single-leaf">
          <v-list-subheader>App status</v-list-subheader>
          <div v-for="app in info.apps">
            <v-dialog v-if="app.errorMessage === null" max-width="500">
              <template v-slot:activator="{ props: activatorProps }">
                <v-list-item
                    :title="app.title ?? app.appDirectory"
                    :subtitle="app.subtitle ?? `Running on port ${app.port}`"
                    v-bind="activatorProps"
                >
                  <template v-slot:prepend>
                    <div class="mr-5" style="font-size: large">✅</div>
                  </template>
                </v-list-item>
              </template>
              <template v-slot:default="{ isActive }">
                <app-dialog :info="app" :ip="info.localIpAddress"/>
              </template>
            </v-dialog>
            <v-list-item
                v-else
                :title="app.title ?? app.appDirectory"
                :subtitle="`Failed to load: ${app.errorMessage}`"
            >
              <template v-slot:prepend>
                <div class="mr-5" style="font-size: large">⚠️</div>
              </template>
            </v-list-item>
          </div>
        </v-list>
      </v-card-text>
      <v-card-text v-else-if="error" class="pa-10 opacity-70 text-center">
        Loading plugin data failed: {{ error }}. Try reloading the page.
      </v-card-text>
      <v-card-text v-else class="pa-10 opacity-70 text-center">
        Loading...
      </v-card-text>
    </v-card>
  </v-container>
</template>
