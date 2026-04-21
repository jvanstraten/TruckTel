# TruckTel: telemetry plugin for ETS2 and ATS

## What?

This is a cross-platform plugin for Euro Truck Simulator 2 and (probably,
untested) American Truck Simulator to bridge the
[SCS telemetry SDK](https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry)
to a REST/websocket webserver. It also supports "semantical input" via that
API. With those things, you should be able to make e.g. a fully functional
browser-based dashboard emulator, and statically host it entirely from the
game.

**TruckTel is not immediately useful for players**, but rather is something
that mod developers can use to get data from the game without actually having
to worry about game internals. So **if you're not a developer, you should
probably stop reading here.**

## Why?

This is hardly the first plugin like this. For example,
https://github.com/Funbit/ets2-telemetry-server is over ten years old. A more
active one is https://github.com/truckermudgeon/scs-sdk-plugin, of which I
admittedly didn't realize the existence of when I started writing this.

Both of these have in common that they rely on relatively low-level
interprocess communication to some other application, which the player then has
to run manually somehow. TruckTel differs in that it runs a webserver straight
from the plugin; all the player has to do is point a web browser, phone app,
or whatever to the right IP address and launch the game.

The plugin uses only code that should work cross-platform. I've tested Windows
and Linux, but have no MacOS system to test a MacOS build on. I did try to get
Actions to build the module for MacOS, but the SCS telemetry header files
immediately complained -- I'm out of my depth here, pull requests welcome.

## What's the project status?

TruckTel should be in a usable state, but it's not well-tested. In terms of
platforms, I can personally only test ATS2 on Linux and Windows. It *should*
work on ATS, but I don't own the game. It *should* work on MacOS (fundamentally
anyway; there might be build issues that need to be resolved), but I don't have
a machine running MacOS. And the APIs *should* all work, but there's no
automated testing (kinda hard to run ETS2 in CI) and I can only test so much
manually. There's no mods using this yet as far as I'm aware, so no real-world
experience either. Until there's a bit more data to go on, I'll keep the
versions in the 0.0.x range.

## How does it work?

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
a local app, not a production server facing the internet. To be clear, do not
open this server to the internet. Its only security is obscurity.

The worker thread allows the server to keep responding while the game is doing
other things, and also decouples any server load from blocking the game thread.
The SCS telemetry API is not thread-safe, so data is still recorded in the game
thread (the code in [`src/recorder`](src/recorder)). The worker thread can then
poll data from the recorders. For frequently-changing data this is just a
single memcpy per frame, so the performance hit to the player should be
negligible.

It's not easily possible to load TruckTel multiple times, in case a player
wants to use multiple TruckTel-based apps at the same time. To account for
this, all app-specific files are scoped in their own directory. TruckTel will
load each of these with their own server thread, each running from its own
port.

Besides running the HTTP/websocket servers, TruckTel responds to mDNS queries
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

## How do I use it?

If you're developing an app and want to use TruckTel as a telemetry source,
this section is for you. If you're a player trying to use an app that depends
on TruckTel, direct your questions to the developers of that app!

TruckTel fundamentally is just a library file: `trucktel.dll` for Windows, or
`libtrucktel.so` for Linux. The library should be placed in the `plugins`
directory of the game for the game to find it. For example, for ETS2 on Linux,
the directory structure should look like this:

```
<game-install-dir>
 |- *.scs                               <- game data and normal mods.
 |- ...                                    TruckTel does NOT go here
 '- bin
     '- linux_x64
         |- eurotrucks2                 <- game executable is here
         |- ...
         '- plugins
             |- libtrucktel.so          <- library goes here!
             '- trucktel
                 |- log.txt             <- log output file
                 |- LICENSE             <- required by dependencies
                 |- mdns.yaml           <- mDNS configuration file
                 '- your-app-name       <- rename for your app
                     |- config.yaml     <- created by the library,
                     |                     but you should provide one
                     '- www             <- document root for static
                         |                 web server
                         '- index.html  <- you should probably put at
                                           least a landing page here
```

What you should probably provide to the user is a zip file containing

 - the libraries for each platform;
 - `trucktel` directory with:
    - `LICENSE` file
    - a directory named after your app, with:
       - `config.yaml`
       - `www` directory with your files, if your app is browser-based
          - `index.html` landing page

along with instructions for the player on where to extract this, or an
installer to automate that. All these files (and a few more) are contained in
the `trucktel.zip` files that you can download from the
[releases](https://github.com/jvanstraten/TruckTel/releases).
Note that TruckTel will also auto-generate these files if they don't exist
yet when it's loaded by the game.

If you nevertheless want to build TruckTel yourself, doing so should be
fairly straightforward. You should only need a recent-ish CMake and a C++
compiler capable of C++17 (batteries may not be included if said compiler is
not GCC or MSVC, because those are the ones tested). You'll find the library
file for your operating system in the CMake build directory. Put it in your
game's plugin directory, run the game, and the default files should all be
generated. Note however that you shouldn't need to recompile TruckTel yourself
if you want to make a mod with it.

The `config.yaml` file specifies which port the plugin should listen on and
what [content types](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Content-Type)
it should serve for static files, based on filename regexes. I added some
default content types, but whether they're sufficient probably depends on
your app and on how picky the player's browser is.

As stated, the plugin serves static files from the `www` subdirectory. The
default file for a directory is `index.html`. If it exists, `/200.html` is
served as a fallback when a file is not found (except for `/api` endpoints).
There are built-in error pages for 404 (not found), 426 (websocket upgrade
required), and 500 (in case I made an oopsie), but you can override them by
providing `/404.html`, `/426.html`, and/or `/500.html` files immediately.
Before serving these error pages, the server will replace all instances of
`%%MESSAGE%%` with the error message.

To actually get data from the game into your app, you can use either REST-like
queries, websockets, or a mix of both. The server serves these at `/api/rest`
and `/api/ws` respectively. For more information, see [api.md](api.md).

## You mentioned licensing?

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

## Bugs/debugging?

The plugin emits a log file in the plugins directory, assuming it loads at all
and it can actually find the plugin directory. If it doesn't, check the in-game
log. To open that:

 - find the game's configuration file:
    - Windows: `Documents/<game name>/config.cfg`
    - Linux: `.local/share/<game name>/config.cfg`
 - set `g_developer` to 1;
 - set `g_console` to 1;
 - once ingame, press `~`.

If there is absolutely nothing in that log relating to attempts to load
plugins, the plugin is probably not installed in the right place, or you're
trying to load a plugin from a different platform entirely (e.g.
`libtrucktel.so` will be entirely ignored on Windows). Otherwise, the game
should tell you what its problem is here.

In general, TruckTel emits log messages to both the log file and the in-game
log. The in-game log is less reliable though, because plugins are only allowed
to log to it in direct response from the game. In the main menu especially, the
game doesn't do much with the plugin, so TruckTel has to queue up log messages
internally.

If you've found a bug that's actually in TruckTel, feel free to fork and/or
make a merge request. Or submit an issue, but my attention span is very
limited, and I probably spent more time developing this plugin than I will ever
spend in-game, so future me might not be particularly helpful.

If you're making a big contribution and want your name added to the license, do
so yourself in the merge request.
