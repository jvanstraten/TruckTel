# Building apps for TruckTel

If you're developing an app and want to use TruckTel as a telemetry source,
this section is for you.

## Directory structure

TruckTel fundamentally is just a library file: `trucktel.dll` for Windows, or
`libtrucktel.so` for Linux. The library should be placed in the `plugins`
directory of the game for the game to find and load it. For example, for ETS2
on Linux, the directory structure should look like this:

```
<game-install-dir>              <- Where Steam takes you for "browse local files"
 |- *.scs                       <- Game data and normal mods. TruckTel does NOT go here.
 '- bin
     '- linux_x64               <- Depends on your OS.
         |- eurotrucks2         <- Game executable; amtrucks for ATS.
         |- ...
         '- plugins             <- Plugin directory. Create if it doesn't already exist.
             '- libtrucktel.so  <- Library goes here!
```

The `trucktel.zip` files in the
[releases](https://github.com/jvanstraten/TruckTel/releases) contain
`trucktel.dll` for Windows and `libtrucktel.so` for Linux, and are meant to be
unpacked into the `plugins` directory.

In addition to the libraries, the zip file contains a `trucktel` directory with
TruckTel's license. This directory is used for much more than that, though:

```
trucktel
 |- LICENSE             <- *TruckTel* license. Do not modify.
 |- log.txt             <- Log output file, written by TruckTel at runtime.
 |- mdns.yaml           <- mDNS configuration file. Defaults are written automatically.
 '- your-app-name       <- TruckTel apps live in their own directory.
     |- config.yaml     <- Default created by the library, but you should provide one.
     |- (license)       <- If you want to add a license file, do so here.
     '- www             <- Document root for static web server.
         '- index.html  <- You should probably put at least a landing page here.
```

If the `trucktel` directory doesn't exist, the license file doesn't exist, the
configuration files don't exist, or no app exists, it will generate defaults
automatically.

## Your app's structure

As said, your app lives in a subdirectory of the `trucktel` directory. For
portability's sake, you should probably name that directory using just
lowercase letters, numbers, dashes, or underscores.

Among other things, that directory contains your `config.yaml` file. Think of
this file as telling TruckTel how to run your app. If the file doesn't exist
yet, TruckTel creates a default one. In addition, if no apps are installed at
all, a placeholder app is generated to help you get started. The default
`config.yaml` file includes all keys, and comments describing what they do.

Most importantly, `config.yaml` specifies which port the server of your app
will be listening on. It should be a high number less than 65535 that should be
unique to your app, otherwise conflicts can arise when multiple apps are
installed. That being said, conflicts can arise anyway. You should ideally
design your app such that the port doesn't matter; in that case, the user can
change it if they have to. For CSR/SPA web apps that should be fairly
straightforward; just use paths relative to the current host only. For native
apps, either give your users the option to change the port number in your app's
settings, or use mDNS service discovery to request the port number for your app
from TruckTel.

Other than that, the `www` directory in your app serves as your document root.
You're free to add additional files below that (i.e. directly in your app's
subdirectory), like instructions or license information. TruckTel won't do
anything with these.

The webserver in TruckTel is very simple: it only serves static files (no SSR
of any kind), only does HTTP, doesn't support compression, etc. The default
file is `index.html` (not `index.htm` etc.), if the URL requested doesn't
specify one. If it exists, `/200.html` is served as a fallback when a file is
not found (except for `/api` endpoints), without generating a 404 error.
Otherwise, there are built-in error pages for 404 (not found), 426 (websocket
upgrade required), and 500 (in case I made an oopsie), which you can override
by providing `/404.html`, `/426.html`, and/or `/500.html` files. Before serving
these error pages, the server will replace all instances of `%%MESSAGE%%` with
the error message using basic string substitution.

## Using telemetry endpoints

To actually get data from the game into your app, you can use either REST-like
HTTP queries, websockets, or a mix of both. The server serves these at
`/api/rest` and `/api/ws` respectively.

TruckTel is quite flexible in what you can do with these endpoints, probably
excessively so, and documenting them here would make this page far too long.
For more information, see [api.md](api.md).

## Example app

There is an example app based on Nuxt [here](../examples/NuxtTruckTelExample/).
This also contains [trucktel.ts](../examples/NuxtTruckTelExample/app/trucktel.ts),
which abstracts away websocket communication. It's meant to be directly
reusable if you're using Vue. For other frameworks you'll probably have to make
adjustments, but it's not overly reliant on Vue and probably serves as a nice
starting point.

You can download a fully-built example app from TruckTel's
[releases](https://github.com/jvanstraten/TruckTel/releases) using the
`trucktel-example.zip` files.

## Distributing your app

What you should probably provide to the user is a zip file containing

 - the libraries for each platform;
 - `trucktel` directory with:
    - `LICENSE` file
    - a directory named after your app, with:
       - `config.yaml`
       - `www` directory with your files, if your app is browser-based
          - `index.html` landing page

along with instructions for the player on where to extract this, or an
installer to automate that. You (or your CI) can download the libraries and
license file using the `trucktel.zip` files in TruckTel's
[releases](https://github.com/jvanstraten/TruckTel/releases).

Some more recommendations:

 - *Do* redistribute TruckTel. You don't need my permission, and it saves users
   from having to install two different things.
 - *Do* include TruckTel's license. I personally don't care, but I used
   3rd-party code licensed under MIT, which requires redistributing their
   license text.
 - *Do not* include files in the `trucktel` directory other than its license
   and your app's subdirectory. TruckTel puts its own configuration files here.
   If a user modifies them and then installs your mod, it'd be a shame if your
   mod would override them.
 - *Do* include at least a basic `index.html` file in your mod's `www`
   directory that identifies your app, even if your app runs natively and
   doesn't use the static web server. In that case, it should instruct your
   users how to start the native app. This is because I might one day add a
   landing page to TruckTel that allows easy redirections to the installed
   apps. A user getting a 404 in that case would be confusing.
