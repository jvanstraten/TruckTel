<script setup lang="ts">

import { reactive, onMounted, onUnmounted } from "vue";
import { TruckTelSocket } from "~/trucktel";

const current_state = reactive<{
    paused: boolean | null,
    time: number | null,
}>({
    paused: null,
    time: 0,
});

const latest_state = reactive<{
    time: number | null,
    engine: boolean | null,
}>({
    time: 0,
    engine: false,
});

const trucktel = new TruckTelSocket("example");
trucktel.current = current_state;
trucktel.latest = latest_state;
trucktel.throttle = 0;
trucktel.dev_host = "192.168.0.196:8081";

onMounted(() => {
    trucktel.open();
});

onUnmounted(() => {
    trucktel.close();
});

</script>

<template>
  <h1>TruckTel demo</h1>
  <p>
    Paused: {{ current_state.paused }}
  </p>
  <p>
    Current time: {{ current_state.time }}
  </p>
  <p>
    Latest time: {{ latest_state.time }}
  </p>
  <p>
    Engine running: {{ latest_state.engine }}
    <button @click="trucktel.pressInput('ignitionoff')">Off</button>
    <button @click="trucktel.pressInput('ignitionon')">On</button>
    <button @click="trucktel.pressInput('ignitionstrt')">Start</button>
  </p>
  <p>
    Note that the game does not acknowledge button input when it doesn't have
    focus, even if it's not paused, so you have to run this web app from a
    different device for the buttons to work.
  </p>
</template>
