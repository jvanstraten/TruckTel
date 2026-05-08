# Troubleshooting TruckTel

If all is working as intended, TruckTel should (by default) open a web browser
with its landing page when the game loads the plugin. There are a bunch of
things that can go wrong here.

 - The plugin is installed in the wrong place, and as a result the game can't
   find it. You're supposed to get a popup in the main menu that reads
   "Request to use advanced SDK features detected." If you don't, check that
   TruckTel is installed correctly.
 - The game might be unable to load the plugin due to some OS issue, or a
   compilation issue with TruckTel. In that case, the game's console should
   show an error message.
 - The game loads TruckTel, but TruckTel itself fails to load. TruckTel should
   also be emitting messages to the game's console, as well as a log file in
   the `plugins/trucktel` directory. Check these for more information.
 - The landing page server might not start, e.g. because the configured port is
   already in use. This will be indicated in TruckTel's log. You can change the
   port assignment in the `landing.yaml` file.
 - The OS might block or doesn't understand TruckTel's request to open a
   browser. This will *not* show up in TruckTel's log. Try going to
   [http://localhost:8079/](http://localhost:8079/) manually (or whichever
   other port is configured, if you reconfigured it) to see if the server is
   running.
 - A firewall might be blocking even localhost loopback traffic, in which case
   the browser might open but not be able to load a page. Note that TruckTel
   only tries to open the browser once the server has successfully started, so
   there really isn't much that could be wrong *other* than firewall
   shenanigans if this happens.

Note that the `landing.yaml` configuration file can also be used to disable the
automatic browser popup, or the whole landing server altogether.

If you're having trouble getting apps to load, but the landing page does load,
follow the troubleshooting instructions on the landing page.

## Opening the game's console

If you need it, the game's development console can be activated and opened as
follows:

 - find the game's configuration file:
    - Windows: `Documents/<game name>/config.cfg`
    - Linux: `.local/share/<game name>/config.cfg`
 - set `g_developer` to 1;
 - set `g_console` to 1;
 - once ingame, press `~`.

## Reporting bugs

If you've found a bug that's actually in TruckTel, and have the knowhow to fix
it, see [contributing](contributing.md). If you don't know how to fix it, you
can [create an issue](https://github.com/jvanstraten/TruckTel/issues).
