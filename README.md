# TruckTel: telemetry webserver for ETS2 and ATS

This is a cross-platform plugin for Euro Truck Simulator 2 and (probably,
untested) American Truck Simulator to bridge the
[SCS telemetry SDK](https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry)
to a REST/websocket webserver. It also supports "semantical input" via that
API. With those things, you should be able to make e.g. a fully functional
browser-based dashboard emulator, and statically host it entirely from the
game.

If you're a player who's having trouble getting a mod or app to work that
uses TruckTel, maybe the [troubleshooting](doc/troubleshooting.md) page can
help. Note that once the landing page opens correctly (popup when you start
the game), you should follow the troubleshooting instructions there.

Otherwise, **TruckTel is not immediately useful for players**, but rather is
something that mod developers can use to get data from the game without
actually having to worry about game internals or low-level interprocess
communication. So **if you're not a developer, you should probably stop
reading here.**

## Why another one?

TruckTel is not the first of its kind. TruckTel included, I'm aware of three
plugin ecosystems like it. Here's a qualitative comparison:

|                         | TruckTel                                                | ets2-telemetry-server                                                                                                                                                                     | scs-sdk-plugin                                                                                                                                                                                                   |
|-------------------------|---------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Links                   | [You are here](https://github.com/jvanstraten/TruckTel) | [Original](https://github.com/Funbit/ets2-telemetry-server) by Funbit, [reference version](https://github.com/Funbit/ets2-telemetry-server/tree/1a5e8abb669b0dc0fa0638f03bed55af120f03cf) | [Original](https://github.com/nlhans/ets2-sdk-plugin) by nlhans, [reference version](https://github.com/truckermudgeon/scs-sdk-plugin/tree/c8910d6e9a5ca0c2fa018942054263292cab19a4) (v1.12.1) by truckermudgeon |
| Telemetry API           | ✅                                                                | ️⚠️ very old (there are probably newer forks) | ️✅                         |
| Input API               | ✅                                                                | ❌                                           | ❌                         |
| Supports Windows        | ✅                                                                | ✅                                           | ✅                         |
| Supports Linux          | ✅                                                                | ❌                                           | ✅                         |
| Supports MacOS          | ❌ (see [#12](https://github.com/jvanstraten/TruckTel/issues/12)) | ❌                                           | ✅                         |
| Data via mmap           | ❌                                                                | ✅                                           | ✅                         |
| Data via websocket/HTTP | ✅                                                                | ⚠️ external application                      | ❌ exist, but not included |
| Generic webserver       | ✅                                                                | ⚠️ external application                      | ❌ exist, but not included |

Where the existing solutions provide a low-level interface between the game and
another native application on the same machine (specifically, memory-mapped
files), TruckTel hosts a webserver with high-level WebSocket and REST APIs
directly from the game. So, if the mod you're building is a mobile app or a
CSR/SPA web application, *your app can run directly from the game*. No
server applications or other install shenanigans required, just have your users
extract a zip file in the right place (or make an installer for it, if you
want). As such, TruckTel specifically targets those kinds of applications. It
should also work just fine for native applications running on the same machine,
but in that case it might be a little overkill compared to the other options.

Figuring out the IP address of the machine running the game can be challenging
to people who are not so tech-savvy. TruckTel helps smoothen the experience in
two ways:

 - A landing page. When TruckTel loads, it will by default open a browser
   (sending the game to the background) which lists all installed TruckTel apps
   and can generate QR codes for opening them on other devices. It also has
   some generic troubleshooting information in it, and a button which tells the
   game to open the TruckTel install directory natively in a file browser.

 - An mDNS server. By default, TruckTel announces itself as being
   `trucktel.local`, with a service record for each app loaded by it. Most
   operating systems nowadays support mDNS resolutions for the `.local` domain,
   so, router, firewall, and LAN party shenanigans aside, `trucktel.local`
   should resolve to the correct IP address once the game is running. Users
   still need to know the port in this case though; while mDNS allows for ports
   to be announced (and TruckTel does this) browsers always just default to
   port 80. For this reason, and because mDNS is much more likely to be blocked
   by firewalls and routers, the QR codes generated by the landing page use
   direct IP addresses. Nevertheless, the mDNS service could be useful for
   native apps.

Finally, there are no existing solutions that I'm aware of that implement the
newer *input* side of the telemetry API. This API allows the plugin to send
input to the game, just like a joystick or keyboard would. Unlike those,
however, the API allows for "semantical" input, where an input might be defined
as `engine` (for toggling engine power) instead of something like `keyboard E`
or `joystick button 3`. That means that you can just make a button or gesture
in your app that toggles engine power, without having to rely on correct key
bindings configured by the user. The semantical input API doesn't support
everything that the game has to offer, but on the other hand, it also supports
things that the ingame key binding configuration *doesn't* offer, like a
three-way switch input for the ignition switch.

Though to be completely honest, I just wanted a way to run
[TruckNav](https://github.com/Rares-Muntean/TruckNav-Sim) on my Linux system
and got a little carried away.

## More information

I wrote far too much, so I split things up into multiple files. Here's an
index:

 - [Troubleshooting TruckTel](doc/troubleshooting.md)
 - [Building apps for TruckTel](doc/app.md)
 - [API documentation](doc/api.md)
 - [Internals](doc/internals.md)
 - [Contributing](doc/contributing.md)
