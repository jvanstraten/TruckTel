# TruckTel internals

The game's plugin system was hardly designed to run a webserver in; plugins are
only really meant to do things in direct response to the game. Fortunately, one
of the things we can do -- in direct response to the game loading the plugin --
is spawning a thread of our own. That thread runs a server based on
[Websocket++](https://github.com/zaphoyd/websocketpp) and
[Asio](https://github.com/chriskohlhoff/asio). Websocket++ is unfortunately not
very actively maintained already at the time of writing this, but it's simple
enough to be header-only with no transitive dependencies that aren't (contrast
e.g. restinio, I think), yet advanced enough to allow for clean asynchronous
shutdown when the game tries to unload the plugin (contrast cpp-httplib, which
was the first thing I tried). Fortunately, this is a game plugin meant to serve
a local app, not a production server facing the internet. To be clear, *do not
open this server to the internet*. I've made reasonable attempts to sanitize
inputs and sandbox things like file access to only the www directory of each
plugin, but I'm sure there are vulnerabilities. Please assume that its only
security is obscurity.

The worker thread allows the server to keep responding while the game is doing
other things, and also decouples any server load from blocking the game thread.
The SCS telemetry API is not thread-safe, so data is still recorded in the game
thread (the code in [`src/recorder`](../src/recorder)). The worker thread can
then poll data from the recorders. For frequently-changing data this is just a
single memcpy per frame, so the performance hit to the player should be
negligible.

It's not easily possible to load TruckTel multiple times, in case a player
wants to use multiple TruckTel-based apps at the same time. To account for
this, all app-specific files are scoped in their own directory. TruckTel will
load each of these with their own server thread, each running from its own
port. Each app has its own configuration file, through which it can select a
port, customize MIME types for the server, customize the JSON structures
returned by the API endpoints, and request which inputs it wants to have access
to. It also has its own "user data" file. This file can be modified and queried
via the websocket endpoints, allowing multiple clients to communicate with each
other or store shared configuration data.

As stated in the features above, TruckTel also responds to mDNS queries
(aka zeroconf aka Avahi aka Bonjour) for `trucktel.local`. That means that,
in theory, so you don't need to know your PC's IP address to connect to it
from a different device on your local network... provided there's no firewall
or router shenanigans that block the queries. It also announces service
information for every app running on TruckTel individually; specifically,
`_trucktel-<app>._tcp`. This could allow e.g. a mobile app to determine the
port number that its associated TruckTel app runs on, allowing players to
reconfigure the port in case of conflict with another service or TruckTel app.

The mDNS subsystem has its own configuration file in TruckTel's main directory.
In particular, this configuration file allows the hostname to be changed from
`trucktel.local` to something else, which would be necessary e.g. for a LAN
party. The mDNS subsystem can also be turned off entirely here in case it gives
issues.

The mDNS system currently intentionally always responds with unicast packets,
even if it's addressed as multicast and the response is expected as being
multicast. As far as I could tell, this didn't have any adverse affects on
mDNS resolvers, while it helped placate Windows firewall somewhat.

## 3rd-party code and licensing notes

I don't really care what you do with my code. I've attached an MIT license to
it, but I'm personally also quite partial to WTFPL, which would require you
to do exactly nothing. There is, however, the matter of the 3rd-party code
I'm using:

 - [SCS telemetry SDK](https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry): MIT
 - [nlohmann::json](https://github.com/nlohmann/json): MIT
 - [fkYAML](https://github.com/fktn-k/fkYAML): MIT
 - [Asio](https://github.com/chriskohlhoff/asio): Boost
 - [Websocket++](https://github.com/chriskohlhoff/asio):
   [custom](https://github.com/zaphoyd/websocketpp/blob/master/COPYING)
 - [mDNS](https://github.com/mjansson/mdns) (slightly modified): Unlicense

So putting a less restrictive license than MIT on my own code wouldn't really
help. None of these are copyleft, but except for Asio they technically require
the license to be bundled also with binary distributions. Websocket++'s license
is even explicit about this.

TruckTel's binary [releases](https://github.com/jvanstraten/TruckTel/releases)
include a full license file with copies of the licenses of all code that was
bundled with it. The source distributions and the repository itself do not
include these licenses, because they don't include the code; CMake fetches it
using FetchContent when configuring.
