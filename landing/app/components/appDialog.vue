<script setup lang="ts">

import VueQrcode from "@chenfengyuan/vue-qrcode";
import type { AppInfo } from "~/types/info";

const { info, ip } = defineProps<{ info: AppInfo, ip: string | null }>();

const url = `http://${ip !== null ? ip : window.location.hostname}:${info.port}/`;

</script>

<template>
  <v-card :title="info.title ?? info.appDirectory" :subtitle="info.subtitle ?? ''">
    <v-card-text>
      <div v-if="info.text !== null" class="opacity-70">{{ info.text }}</div>
      <div v-if="info.link !== null"><a :href="info.link" class="opacity-70">{{info.link}}</a></div>
      <div v-if="!info.disableLauncher">
        <v-divider class="mt-5"/>
        <div v-if="ip !== null">
          <div class="mt-5 mb-2 text-center">Click or scan to launch:</div>
          <div class="rounded-xl overflow-hidden bg-white d-flex justify-center"><a :href="url"><vue-qrcode :value="url" :options="{ width: 450 }"/></a></div>
          <div class="mt-2 text-center"><a :href="url">{{url}}</a></div>
        </div>
        <div v-else>
          <div class="mt-5 text-center">Click to launch locally:</div>
          <div class="mt-2 text-center"><a :href="url">{{url}}</a></div>
          <v-divider class="mt-5"/>
          <div class="mt-5 opacity-70">
            Note: TruckTel failed to determine your local IP address, and therefore
            can't generate an URL or QR code that will work on another device on
            your network. Ensure that you're actually connected to a network and
            have an IP address!
          </div>
        </div>
      </div>
    </v-card-text>
  </v-card>
</template>
