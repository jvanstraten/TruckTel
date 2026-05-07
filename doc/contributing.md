# Contributing

Did you fix a bug, solve an issue, or update game support? Or maybe you've
added a feature? Feel free to make a pull request, and I'll try to have a look.
If you're making a big contribution and want your name added to the license,
please do so yourself in the merge request. Note that, to keep things clean and
consistent, this repository uses pre-commit hooks for code formatting. CI also
checks adherence, but you'll definitely save yourself a lot of time by running
the hooks locally. The pre-commit system is from Python, so (install Python
and pip and) run:

```
python -m pip install pre-commit
python -m pre-commit install
```

Now when you commit, pre-commit should do its thing.

## Building

Building TruckTel should be straightforward. You should only need a recent-ish
CMake and a C++ compiler capable of C++17 (batteries may not be included if
said compiler is not GCC or MSVC, because those are the ones tested). You'll
find the library file for your operating system in the CMake build directory.
Put it in your game's plugin directory, run the game, and the default files
should all be generated.

Note that CMake pulls in all third-party code using FetchContent. If the links
break, you'll need to swap them out with working ones.

## Forking

Historically, my attention span with things like this hasn't been all that
great, so it's unlikely I'll be actively maintaining the plugin myself (ideally
it also doesn't *need* a lot of maintenance to begin with), but I will try to
respond to PRs. If I nevertheless don't for some reason, feel free to fork the
repository and take over.

If you *do* fork the repository and make breaking changes, you might want to
change the name of the libraries and, more importantly, the directory they load
their configuration data from. That should help avoid conflicts in case a user
would want to install two incompatible versions of TruckTel side by side. This
name can be chaned in [version.h.in](../src/version.h.in).
