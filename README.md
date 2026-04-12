# INCOMPLETE

This may one day become a plugin for Euro Truck Simulator 2 (and probably
American Truck Simulator) to present the telemetry provided by the game
engine via the [SCS telemetry SDK](https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry)
via REST, websocket, or something similar.

Basically, the same thing as https://github.com/Funbit/ets2-telemetry-server,
except:

 - cross-platform, or at least supporting Linux because Funbit's code does not;
 - just the raw data via JSON messages, no fancy HTML UI (not my expertise).

with the intention to get https://github.com/Rares-Muntean/TruckNav-Sim to
work on Linux.
