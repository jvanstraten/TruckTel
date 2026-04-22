<script setup>

import { reactive, onUnmounted } from "vue";
import { TruckTelSocket } from "~/trucktel.js";

const current_state = reactive({
    paused: null,
    time: null,
});

const latest_state = reactive({
    time: 0,
    engine: false,
});

const trucktel = new TruckTelSocket("example");
trucktel.current = current_state;
trucktel.latest = latest_state;
trucktel.throttle = 0;
trucktel.dev_host = "localhost:8081";
trucktel.open();

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
    <button @click="trucktel.pressInput('engine')">Toggle</button>
  </p>
</template>
