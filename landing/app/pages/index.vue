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

function getPort() {
  return window.location.port;
}

async function browseInstallDir() {
  try {
    const response = await fetch("/api/browse-install-dir", { signal: AbortSignal.timeout(1000) });
    if (!response.ok) {
      alert("API did not respond");
    } else {
      const path = await response.json() as string;
      alert(`In case your file browser didn't open, the install directory is: ${path}`);
    }
  } catch (e) {
    alert(`${e}`);
  }
}

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
          <span v-if="info.localIpAddress">
            at {{ info.localIpAddress }}<span v-if="info.mdnsHostname">
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
                :subtitle="`Failed to load (port ${app.port}): ${app.errorMessage}`"
            >
              <template v-slot:prepend>
                <div class="mr-5" style="font-size: large">⚠️</div>
              </template>
            </v-list-item>
          </div>
        </v-list>
        <v-divider/>
        <v-list lines="one" select-strategy="single-leaf">
          <v-list-subheader>Help</v-list-subheader>
          <v-dialog max-width="500">
            <template v-slot:activator="{ props: activatorProps }">
              <v-list-item title="What is this?" v-bind="activatorProps">
                <template v-slot:prepend>
                  <div class="mr-5" style="font-size: large">😕</div>
                </template>
              </v-list-item>
            </template>
            <template v-slot:default="{ isActive }">
              <v-card title="What is this?">
                <v-card-text>
                  <p>
                    This is the landing page for TruckTel, a telemetry plugin for Euro Truck Simulator 2 and American
                    Truck Simulator. You can think of it like a mod loader for mods or apps running outside the game.
                    It provides the scaffolding for actual mods to access telemetry from the game. Telemetry like your
                    truck's position, for instance.
                  </p>
                  <p>
                    This page is normally opened by the game automatically after you install an app or mod that uses
                    TruckTel. The idea is that you can use this page to quickly get a QR code to open the app on your
                    phone, or a link to open it on a second screen.
                  </p>
                  <p>
                    If you don't want this page opening automatically every time you start the game, you can stop this
                    page from opening automatically by opening the landing.yaml configuration file in TruckTel's install
                    directory in a text editor, and replacing "auto-open: true" with "auto-open: false". You can also
                    uninstall TruckTel entirely, of course, but then you also lose the apps running on it. To do that,
                    go to your game's plugin directory, and delete the files and folder with "trucktel" in the name.
                  </p>
                </v-card-text>
                <v-card-actions>
                  <v-spacer/>
                  <v-btn @click="isActive.value = false">Close</v-btn>
                </v-card-actions>
              </v-card>
            </template>
          </v-dialog>
          <v-dialog max-width="800">
            <template v-slot:activator="{ props: activatorProps }">
              <v-list-item title="Troubleshooting" v-bind="activatorProps">
                <template v-slot:prepend>
                  <div class="mr-5" style="font-size: large">💥</div>
                </template>
              </v-list-item>
            </template>
            <template v-slot:default="{ isActive }">
              <v-card title="Troubleshooting">
                <v-expansion-panels flat>
                  <v-expansion-panel title="App isn't working"><v-expansion-panel-text>
                    <p>
                      If a TruckTel app is loading, or even just seems to be trying to load, any issue with it is
                      unlikely to be TruckTel's fault. Look for troubleshooting information specific to the app you're
                      having issues with.
                    </p>
                    <p>
                      If an app isn't loading at all, read the "app isn't loading" item below.
                    </p>
                  </v-expansion-panel-text></v-expansion-panel>
                  <v-expansion-panel title="QR code isn't working"><v-expansion-panel-text>
                    <p>
                      Let's first distinguish between the camera app not recognizing the QR code, and a browser
                      opening but failing to load the page. In the former case, you can try typing the URL below the
                      QR code into your device's browser manually -- the QR code encodes exactly that and is just a
                      convenience thing. If your browser does open but the page doesn't load, read the next section.
                    </p>
                  </v-expansion-panel-text></v-expansion-panel>
                  <v-expansion-panel title="App isn't loading"><v-expansion-panel-text>
                    <p>
                      Make note of the error message shown by your browser:
                    </p>
                    <ul>
                      <li>
                        Connection refused or timeout: probably a firewall issue. To be sure, check to see if the app
                        <i>does</i> load on this device, by clicking the QR code or the link beneath it.
                        <ul>
                          <li>
                            If the app does work here, and "here" is the computer you're running the game on (that is,
                            no other device has worked so far) it's almost certainly a firewall issue. Make sure to
                            allow the game to access your local network in your firewall settings. What firewall that
                            is and where those settings are depends on your OS and whether you have a 3rd-party virus
                            scanner installed, so you'll have to do some googling yourself. I would recommend
                            (temporarily!) disabling your firewall and virus scanners to see if that actually fixes
                            the problem, and only then try to make more fine-grained exceptions; you can waste a lot
                            of time chasing ghosts otherwise. If disabling your firewall doesn't fix the issue, check
                            the steps for the "address unreachable" symptom below.
                          </li>
                          <li>
                            If the app does work here, and "here" is NOT the computer you're running the game on, it's
                            probably not a firewall issue, but a network or router issue. Check the steps for the
                            "address unreachable" symptom below.
                          </li>
                          <li>
                            If the app doesn't work here either, probably there's a bug in TruckTel. The log might
                            help shed light on it. You can find it in TruckTel's installation directory.
                          </li>
                        </ul>
                      </li>
                      <li>
                        Address unreachable: probably your computer and your mobile device are not connected to the
                        same network, or they're both connected to a network that doesn't allow devices to communicate
                        with each other (this is usually the case for guest networks). Make sure both devices are
                        connected to the same WiFi network and that your router is configured to allow connections
                        between devices. Restart the game after you make changes to your network settings.
                      </li>
                      <li>
                        File not found: this is the app's fault. The app might not host a web app from within TruckTel
                        directly, and instead require you to install something on your devices natively, but in that
                        case the author should disable the launcher in their app's configuration file. It could also
                        be that something went wrong during installation of the app, e.g. not all files were installed
                        correctly. If the app isn't native-only, try reinstalling it.
                      </li>
                      <li>
                        Some form of security error or HTTPS-only mode complaint: you're going to have to make an
                        exception in your device's browser, or use a different browser if it doesn't let you. This
                        does not mean TruckTel is unsafe or not secure; lack of HTTPS means that you shouldn't, for
                        instance, enter your credit card number, but hopefully you weren't intending to do that for
                        an ETS2/ATS mod anyway. Implementing HTTPS for connections on your local network is
                        practically impossible, because that's simply not what HTTPS is for, but some browsers are a
                        bit overzealous in their security policies.
                      </li>
                    </ul>
                  </v-expansion-panel-text></v-expansion-panel>
                  <v-expansion-panel title="App settings keep resetting"><v-expansion-panel-text>
                    <p>
                      If the app you're using stores configuration data on your device, it could be that your
                      configuration data is getting lost because your local IP address is changing. This is because
                      local storage in browsers is scoped to the server that the browser thinks it's talking to.
                      Otherwise, any odd website could read the data from any other website -- imagine if a random
                      ad could read a credit card number stored by a trusted webshop! In case of TruckTel apps, the
                      only thing your browser has to go on about which server it's talking to is your IP address, and
                      some routers have a tendency to pick a new one at random every time you restart your computer.
                    </p>
                    <p>
                      If your local IP address does indeed change when you restart your computer, you can try to use
                      "trucktel.local" instead of the QR codes. In modern devices anyway, .local domains will be
                      resolved via mDNS, which TruckTel runs a server for. For example, if the app's QR code links you
                      to "http://192.168.0.1:3000/", replace the IP address in your browser's URL bar like so:
                      "http://trucktel.local:3000/". If that works, you can bookmark the page in your device's
                      browser, because the link will then always be the same.
                    </p>
                    <p>
                      mDNS isn't used in the QR codes because it is relatively likely for routers or firewalls to
                      block mDNS, unless you explicitly allow mDNS. Unlikely scenario, but it also wouldn't work right
                      in case you're doing a LAN party or something, and there are multiple people who use TruckTel on
                      the same network. So if it works, it works; if not, it's usually not worth the hassle trying to
                      get it to work.
                    </p>
                    <p>
                      You can also try to configure a static IP address for your computer, either with your computer
                      or in your router settings. You'll have to consult google for that, since it depends heavily on
                      your operating system, router make/model, and network settings.
                    </p>
                  </v-expansion-panel-text></v-expansion-panel>
                  <v-expansion-panel title="Port/address already in use"><v-expansion-panel-text>
                    <p>
                      TruckTel hosts a webserver for each app individually. A computer (normally) only has one IP
                      address, so an additional number is needed to identify which webserver you're trying to talk to.
                      This is the number after the colon in the URL bar. For example, this page was loaded from port
                      {{ getPort() }}.
                    </p>
                    <p>
                      If two TruckTel apps (or services on your computer in general) try to use the same port, it's
                      first-come first-serve; the second app won't be able to start. That's what the message means.
                    </p>
                    <p>
                      Ports are configured in the config.yaml file of each TruckTel app. Click the "browse install
                      directory" button on the main page, open the directory named after the app that isn't working,
                      and open its config.yaml file. The port number should be near the top. Change it to something
                      similar (but different) until it works. Port numbers should be whole numbers between ~1000 and
                      65535. Higher numbers are more likely to work.
                    </p>
                  </v-expansion-panel-text></v-expansion-panel>
                  <v-expansion-panel title="Unknown IP address"><v-expansion-panel-text>
                    <p>
                      TruckTel normally displays your local IP address at the top of this page. It's possible,
                      however, that TruckTel cannot figure out what your IP address is. In that case, this page also
                      won't be able to generate links or QR codes for launching apps on mobile devices.
                    </p>
                    <p>
                      The most likely reason for the failure is not being connected to your WiFi or wired network, so
                      check your network settings! Note that you need to restart the game for information on this page
                      to update.
                    </p>
                    <p>
                      It could in theory also be a permissions issue, in which case you might still be able to connect
                      with a mobile device by entering your computer's IP address manually. If you know your
                      computer's IP address, you can try navigating to http://[your-ip-address]:{{ getPort() }}/ on
                      the device you want to load an app on. Assuming this was your only problem, this landing page
                      should load on your device, after which you can launch the desired app locally.
                    </p>
                  </v-expansion-panel-text></v-expansion-panel>
                </v-expansion-panels>
                <v-card-actions>
                  <v-spacer/>
                  <v-btn @click="isActive.value = false">Close</v-btn>
                </v-card-actions>
              </v-card>
            </template>
          </v-dialog>
          <v-list-item title="Browse install directory" @click="browseInstallDir">
            <template v-slot:prepend>
              <div class="mr-5" style="font-size: large">📁</div>
            </template>
          </v-list-item>
          <v-list-item title="Visit https://github.com/jvanstraten/TruckTel" href="https://github.com/jvanstraten/TruckTel">
            <template v-slot:prepend>
              <div class="mr-5" style="font-size: large">🔗</div>
            </template>
          </v-list-item>
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

<style scoped>

ul {
  opacity: 0.9;
  padding-left: 25px;
}

li {
  margin-top: 5px;
}

</style>