# Debugging TruckTel

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

If you've found a bug that's actually in TruckTel, and have the knowhow to fix
it, see [contributing](contributing.md).
