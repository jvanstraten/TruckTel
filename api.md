# API documentation

## Data structures and concepts

TruckTel makes all information provided by the SCS telemetry API available
as-is, with only conversions to JSON. Therefore, it's helpful to know some
basics about how the game presents telemetry data.

### Scalar data types

At the lowest level, the telemetry API supports the following types of values.

| SCS type   | JSON representation | Notes                             |
|------------|---------------------|-----------------------------------|
| bool       | boolean             |                                   |
| s32        | integer             |                                   |
| u32        | integer             |                                   |
| s64        | integer             |                                   |
| u64        | integer             |                                   |
| float      | float               |                                   |
| double     | float               |                                   |
| fvector    | array of 3 floats   | \[x, y, z\]                       |
| dvector    | array of 3 floats   | \[x, y, z\]                       |
| euler      | array of 3 floats   | \[heading, pitch, roll\]          |
| fplacement | array of 6 floats   | \[x, y, z, heading, pitch, roll\] |
| dplacement | array of 6 floats   | \[x, y, z, heading, pitch, roll\] |
| string     | string              |                                   |
| version    | array of 2 integers | not part of `scs_value_t`         |

Coordinate system:

 - local: X points to right, Y up and Z backwards;
 - world: X points to east, Y up and Z south.

Euler orientation conventions:

 - heading: stored in unit range where <0,1) corresponds to <0,360). The angle
   is measured counterclockwise in horizontal plane when looking from top where
   0 corresponds to forward (north), 0.25 to left (west), 0.5 to backward
   (south) and 0.75 to right (east).
 - pitch: stored in unit range where <-0.25,0.25> corresponds to <-90,90>. The
   pitch angle is zero when in horizontal direction, with positive values
   pointing up (0.25 directly to zenith), and negative values pointing down
   (-0.25 directly to nadir).
 - roll: stored in unit range where <-0.5,0.5> corresponds to <-180,180>. The
   angle is measured in counterclockwise when looking in direction of the roll
   axis.

### Indexed values

Some values presented by the game are "indexed." Such values behave like arrays
of the scalar data types above, and are represented in JSON as arrays.

Because of the way the game represents values, it is technically possible for
an array to be sparse, or for it to have different data types in it. In
practice this probably never happens, but it's worth considering. If TruckTel
has to emit a sparse array, unpopulated entries are filled with JSON null.

### Keys

The game presents data or allows monitors to be registered on values based on
period-separated keys. For example, `truck.world.placement` is used to
reference the placement of a reference point of the truck in world space.

The game distinguishes two kinds of data sources with slightly different
semantics for these keys:

 - channels: used for dynamic, rapidly changing data. The game provides a value
   for each simulation frame for each channel that the telemetry plugin
   requests updates for. TruckSim requests all of them, as far as the header
   files from SCS name them. There could be more, now or in the future, but
   they would have to be added to TruckTel's source code for them to work.
 - configuration: used for data that changes infrequently, like information
   about the current job, or the truck's gearbox. This information is pushed
   to the plugin by the game; if SCS decides to add more information, TruckTel
   will get that new information without changes to its source code. Keys work
   slightly differently in that a "configuration" consists of a number of
   key-value pairs, on top of a key that identifies the configuration block
   itself. TruckTel concatenates these keys; you won't notice the difference
   unless you dive into the SCS documentation.

Note that channels and configurations may be unavailable at times, based on
context. Requesting an unavailable channel via the API yields an empty object
or null, depending on what makes more sense in context.

There are a few side channels that provide information to the plugin via
different ways:

 - initialization function parameters: provide information about the game.
   Trucktel maps this data to `game.name`, `game.id`, `game.version`, and
   `game.api_version`.
 - frame timing information passed via the `frame_start` event. TruckTel maps
   this data to `frame.render_time`, `frame.simulation_time`, and
   `frame.paused_simulation_time`. It also derives `frame.fps` and
   `frame.fps_filtered` from this information.
 - pause/unpause state passed indirectly via the `paused` and `started` events.
   TruckTel presents the paused state via `frame.paused`.
 - current working directory: used to derive the game installation directory,
   presented via `game.install_dir`.

TruckTel presents all data sources to its web API in the same way and in the
same namespace. This relies on there not being conflicts between configuration
and channel data. Some configuration attribute names are modified by TruckTel
to avoid this. These are indicated with an asterisk in the tables below. This
is not automated; if SCS adds attributes and these conflict with a channel,
then the channel takes precedence.

All of this results in a flat key-value data structure, where the keys have
period separators. But you might also want to think about this in a more
structured way, with nested key-value maps/JSON objects. Unfortunately, this
only half works, because there are many instances where the game will expose
a value for both `x` and `x.y`, for example `truck.adblue` and
`truck.adblue.capacity`. This is a problem because `x` would need to be both
a value and a nested object. Whenever this happens, TruckTel will "replace"
the `x` key with `x._` to make the nesting work. Both this and the flat view
have benefits and drawbacks, so the API supports both forms.

### Custom structures

Don't like either of the flattened or structured formats? Me neither. Define
your own! You can specify these custom structures in the `custom_structures`
key in TruckTel's `config.yaml` file.

`custom-structures` must point to an object. The keys of this object are what
you pass to the `<structure>` path element of the server endpoints (defined
in the server endpoint section below). The value can be any pile of nested
objects or arrays you like, and defines the resulting structure of the JSON
object, except instead of the values you want to see, you define yet another
object that defines where TruckTel should take the data from.

Suppose you want horizontal coordinates in whole meters, heading in whole
degrees, and the in-game time for some reason represented as a date. You can
then write in `config.yaml`:

```yaml
custom-structures:
  gps:
    coord:
      x:
        key: truck.world.placement.0
        operator: round
      y:
        key: truck.world.placement.1
        operator: round
      z:
        key: truck.world.placement.2
        operator: round
    heading:
      key: truck.world.placement.3
      scale: 360
      operator: round
    time:
      key: game.time
      operator: date
```

and then request e.g. `http://<server>:<port>/api/ws/delta/gps?throttle=0` to
get structures that look like this:

```json
{
  "coord": {
    "x": -20328,
    "y": 27,
    "z": -6461
  },
  "heading": 192,
  "time": "0001-01-01T15:16:00Z"
}
```

This will only take as much bandwidth as it needs to, especially with
delta-coding applied to rounded values; even at standstill, the game engine
is very noisy.

Note that the internal conversion process isn't particularly well-optimized.
TruckTel does all the things it would do for the `struct` query, which is
already a bit janky, and then builds an entirely new structure from that by
interpreting your custom configuration structure. That said, this is still
C++. My gut feeling is that it'll be faster than dealing with the full JSON
struct with everything in it in a scripted language on a phone, or than
opening a bunch of websockets to filter data that way.

As stated, the resulting structure can be any hierarchy of objects and arrays.
Structural objects are disambiguated from format specification objects by
checking if any of the elements of the object are arrays or objects.

If a format specification object is valid, it will be replaced with the data
it resolved. If the referenced data does not exist, the replacement value will
be null. If the format specification object is itself invalid, the replacement
value will be a string with the error message.

Format specification objects must have either a `static` or a `key` key. If
`static` is specified, its value will be the replacement value. This is only
ever going to be useful for mimicking other people's plugins; normally you'd
specify `key`. `key` must then be a period-separated string that internally
(hierarchically) indexes into the default structural data format.

Unless otherwise specified, the resolution will prefer single values over
structures of "child" values. For example, if you ask for `truck.oil.pressure`,
you get just the oil pressure. You can override this by adding `struct: true`
to the format specification object, in which case you'll get for instance:

```json
{
  "_": 0.0,
  "warning": {
    "_": true,
    "threshold": 10.149999618530273
  }
}
```

You can also let TruckTel do some post-processing on the selected data. This
could be used to mimic the data formats presented by competing plugins, or to
reduce traffic in delta-coded websocket connections. The following
post-processing operations are currently defined, and are applied in the
listed order if multiple are present.

 - `offset: <desired offset>`: offsets a number by the given number. Mostly
   there to facilitate number range checks via `offset`, `scale`, and `zero`.
 - `scale: <desired scale factor>`: scales a number by the given number. This
   could be used, for instance, to convert m/s to km/h, rotations to degrees,
   or fractions to percentages. Scaling is applied before operators, in case
   both are specified.
 - `operator: round`: rounds to the nearest integer for numeric values. Useful
   to avoid update spam for values that change only marginally due to noise.
 - `operator: bool`: returns a reasonable boolean representation of the value:
    - objects, arrays, strings: returns non-empty.
    - numbers: returns whether the number *rounds to* nonzero. Use scaling and
      offsetting to shift the margins for which numbers you want to be treated
      as zero.
    - null: left as null.
 - `operator: zero`: returns the complement of `bool`.
 - `operator: date`: a cursed operator that converts a game time in minutes to
   an ISO 8601 date, starting from `0001-01-01T00:00:00Z`. This exists for
   compatibility with some plugins that like to do this. Maybe the game also
   starts counting at year 1? I don't know.

It might be argued that I went a little overboard with these functions.

### (Gameplay) events

In addition to channels and configurations, the game also provides a stream of
gameplay events. Events are ephemeral or instantaneous things, like the player
receiving a fine for something. The game uses a separate event system to report
pause/unpause events, but TruckTel unifies this into a single event stream.

Events have an ID that identifies the type of event, and zero or more named
attributes associated with them. The JSON representation for events is much the
same as it is for channel and configuration data: a JSON object representing a
key-value store, in either the flat or hierarchical form. The event ID is added
to the object via a `_` key in the toplevel object.

### Semantical input

TruckTel can optionally also provide access to the SCS input API. This allows
you to control certain things in the game via REST and websockets as well. For
instance, you could make a complete virtual dashboard with functional buttons
this way.

The SCS input API supports two types of input devices: generic and semantical.
The difference is that, for generic devices, inputs pass through the game's
input mapping configuration, while for semantical, game inputs are controlled
directly. For example, the semantical channels `lblinkerh` and `rblinkerh`
could be tied to a physical blinker stalk. A generic device would instead be
something like a controller, with general-purpose buttons, switches, and axes.
TruckTel currently implements a semantical input device only, because this
makes more sense for the intended use case of web pages or touchscreen apps
that are designed specifically for ETS2/ATS.

The game refers to input channels as "mixes". I guess this originates from the
way their input mapping configuration works: via some limited expression
support, several kinds of inputs are "mixed" together. For instance, you might
be able to control the camera both with the mouse, a controller, and the
semantical channels `camlr` and `camud` simultaneously. The input configuration
file determines how these three inputs would be mixed.

Semantical inputs can be either binary or floating-point axes. TruckTel
supports both.

Unfortunately, this is mostly where the API documentation ends. This is all it
has to say on the subject of which inputs are supported and how they are named:

> Note that only subset of mixes are supported. If mix expression
> in a fresh controls.sii references something like "semantical.<mixname>?0",
> then semantical input is likely supported for that mix.

The documentation also doesn't state what a reasonable value range for a float
input is. You might expect it to be normalized to <0,1> or <-1,1>, but for
camera control at least, that yields very slow movement. The limits are
arbitrarily set to <-100000,100000> in TruckTel.

At the bottom of this page is a list of semantical input IDs that I manually
pulled from the default ETS2 1.18 configuration file. For some inputs it's
fairly easy to guess at what they do, for others not so much. You'll have to
do your own reverse-engineering.

Unlike data channels and configuration, TruckTel doesn't just register every
known semantical input channel. You have to configure which channels you want
in `config.yaml` yourself. With the default `config.yaml`, the input subsystem
is disabled altogether.

The main reason for this is that it turns out that merely registering an input
will change the behavior of the game to the point where, with the default
config, you can't even manually start the truck anymore. Normally the E key is
tied to `engine` and starts both the engine and electronics simultaneously, but
merely binding an input to `engineelect` changes the behavior of the `engine`
input to control *only* the engine, with only the TruckTel API being bound to
turning on the electronics.

Another reason is that the game wants a player-friendly name for inputs (this
is what it uses in hint popups), and for most of the inputs I have no idea what
a reasonable name would be.

The `config.yaml` structure is straightforward. Here's an example:

```yaml
input:
  float:
    camlr: Camera yaw axis
    camud: Camera pitch axis
  binary:
    lblinkerh: Left blinker switch
    rblinkerh: Right blinker switch
    engine: Toggle engine
```

The `float` object specifies floating-point inputs (axes); the `binary`
object specifies binary inputs (buttons and switches) in the same format. Said
format is just an object, of which the keys are the input identifiers, and the
values are the player-friendly names.

If the game doesn't like one or more of the channels you've defined, it will
emit an error message in the in-game log with a numeric index. TruckTel will,
in turn, emit a mapping from numeric index to identifier to its log file (and
only the log file, so as not to spam the in-game log).

## Server endpoints

Your app can get data from TruckTel either via normal HTTP requests (REST) or
via websockets.

### REST data access

Data in the channel and configuration namespace can be requested via HTTP
queries of the following form:

```
http://<server>:<port>/api/rest/<structure>/<key...>
```

`<key...>` is one or more path elements that map to a data key. For consistency
with URL paths, slashes may be used instead of periods.

`<structure>` defines how the data is represented, and must be one of:

 - `struct`: for the hierarchical data representation described above.
 - `flat`: for the flattened data representation described above.
 - `single`: a single value is returned, without its key being present in the
   resulting JSON.

For example, we can request `truck.world.placement` in three different ways:

```
http://<server>:<port>/api/rest/struct/truck/world/placement:
                                ‾‾‾‾‾‾
    {
      "truck": {
        "world": {
          "placement": [...]
        }
      }
    }

http://<server>:<port>/api/rest/flat/truck/world/placement:
                                ‾‾‾‾
    {
      "truck.world.placement": [...]
    }

http://<server>:<port>/api/rest/single/truck/world/placement:
                                ‾‾‾‾‾‾
    [...]
```

Note that JSON is returned in minified form; the indentation shown in the
documentation is added manually.

For the `flat` and `struct` representations only, all keys matching the given
prefix are returned. For example,

```
http://localhost:8080/api/rest/struct/truck/light:

    {
      "truck": {
        "light": {
          "aux": {
            "front": 0,
            "roof": 0
          },
          "beacon": false,
          "beam": {
            "high": false,
            "low": false
          },
          "brake": false,
          "lblinker": false,
          "parking": false,
          "rblinker": false,
          "reverse": false
        }
      }
    }
```

In the most extreme case, you can just request all data in one go this way.

### Websocket data access

The REST endpoints above require a web application to poll the server
constantly, without knowing whether new data is even available. This is
probably not hugely performant. For more performance, the websocket APIs can
be used instead.

The URL structure when opening a connection is mostly the same as it is for
the REST endpoints:

```
http://<server>:<port>/api/ws/<data-type>/<structure>/<key...>
                           ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
```

where `<data-type>` can be one of three things:

 - `data`: the data returned via the websocket connection is exactly the same
   as what you'd get by repeatedly querying the equivalent REST endpoint,
   except that TruckTel continuously pushes data to your application on its
   own, until you close the connection.
 - `delta`: the same data as for `data`, but objects are delta-coded (defined
   below).
 - `event`: instead of normal data, you get a stream of events. Each event is
   its own JSON object. `<key...>` is not used for this data type, and must be
   empty.

In addition to the path, you can append a `?throttle=<millis>` query string.
This controls how often the server will send updates: it will drop a data
update when fewer than `<millis>` milliseconds have passed since the previous
update. `throttle` defaults to 1000 for `data` and `delta` and to 0 for `event`
(since throttling doesn't make much sense there). If you disable throttling,
you'll get an update for every frame reported to the plugin by the game, or at
least every second (when the game is not generating frames).

Delta-coding can be used to stop TruckTel from sending data when the data has
not changed. Before sending data (after throttling), it will compare the
current data against the data that was most recently reported to the client
recursively for all JSON objects, and:

 - new keys are always reported;
 - keys for which the value has changed are reported, with a recursive
   delta-coding check if both the new and old value are objects;
 - keys for which the value has not changed are omitted entirely;
 - removed keys are reported via a key set to null.

Delta-coding would be useful for instance for `truck.light`: your app will get
a message only when the user toggles a light, usually immediately (regardless
of throttling), unless the player is playing with their truck's lights or
something.

### REST input control

The virtual input subsystem can be controlled via HTTP queries of the following
forms:

```
http://<server>:<port>/api/rest/input/list
http://<server>:<port>/api/rest/input/press/<input-id>
http://<server>:<port>/api/rest/input/hold/<input-id>
http://<server>:<port>/api/rest/input/release/<input-id>
http://<server>:<port>/api/rest/input/set/<input-id>/<float-value>
```

The `list` command returns a JSON object of which the keys are all the input
IDs that are currently configured, and the values are either `"binary"` or
`"float"` to indicate the channel type. For example:

```json
{
   "camlr": "float",
   "camud": "float",
   "lblinkerh": "binary",
   "rblinkerh": "binary"
}
```

The `press`, `hold`, and `release` commands are meant to be used to control
binary channels. `press` will assert the input for one frame, and then
automatically release it again. If necessary (because you're spamming input
and/or the game is running in slideshow mode on a potato), TruckTel will queue
up button presses for a channel. `hold` will assert the input without releasing
it automatically, until either a `press` or `release` command is given.

The `set` command is meant for floating-point channels/axes. It should speak
for itself.

Except for `input`, the JSON return value of the query will normally be `true`,
to indicate that the event was queued up successfully. It will be `false` if
the channel does not exist. If something else goes wrong, it will be an error
message as a JSON string.

### Websocket input control

Any websocket you open can control inputs. Simply send JSON arrays of strings
representing the REST query path after the `input` element. For example,

```json
["press", "engine"]
```

would simulate a button press that toggles whether the Truck's engine is
enabled.

The websockets used for monitoring data or events will not reply to input
messages, to avoid ambiguity of the messages. That is, you won't get to see if
a command was executed successfully. Usually this shouldn't matter; there's no
good reason why it should fail other than misconfiguration.

Nevertheless, if you do want to know, for example for debugging, you can
connect to the following endpoint:

```
http://<server>:<port>/api/ws/input
```

TruckTel will not send any messages on this websocket, except in direct
response to an input message that it receives. The response will be equivalent
to that of an HTTP input command.

## Data reference

For detailed documentation,
[download the SCS telemetry API](https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry)
and look at the docstrings in `include/common`. With the background information
above, you should be able to understand those files without knowing anything
about C++.

### Channel and configuration data

Notation:

 - In some cases, two similar keys are merged into one in the tables using
   `[x/y]` notation: this means that this part of the key can be either `x` or
   `y`.
 - Trailers are sort of but not really an array in the game; I don't know the
   dev history of the game, but it seems like the notion of multiple trailers
   is an afterthought. You can get information about the trailers via
   `trailer.<index>.<key>`, where `index` is a number from 0 to 9. This is
   represented in the tables below as simply `trailer.#.<key>`. Note that these
   numbers are stored as JSON object key strings in the structured format, NOT
   as array indices.

#### Game identification

| Key                | Type    | Description                                                        |
|--------------------|---------|--------------------------------------------------------------------|
| `game.name`        | string  | Localized name of the game.                                        |
| `game.id`          | string  | ID of the game, currently either `eut2` for ETS2 or `ats` for ATS. |
| `game.version`     | version | Game major and minor version.                                      |
| `game.api_version` | version | SCS telemetry API version.                                         |
| `game.install_dir` | string  | Installation directory of the game.                                |

#### Controls

| Key                             | Type   | Description                                                                                                                                                                                        |
|---------------------------------|--------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `truck.input.steering`          | float  | Steering received from input <-1;1>, counter-clockwise.                                                                                                                                            |
| `truck.effective.steering`      | float  | Steering as used by the simulation <-1;1>, counter-clockwise. Accounts for interpolation speeds and simulated counterforces for digital inputs.                                                    |
| `truck.input.throttle`          | float  | Throttle received from input <0;1>.                                                                                                                                                                |
| `truck.effective.throttle`      | float  | Throttle pedal input as used by the simulation <0;1>. Accounts for the press attack curve for digital inputs or cruise-control input.                                                              |
| `truck.input.brake`             | float  | Brake received from input <0;1>.                                                                                                                                                                   |
| `truck.effective.brake`         | float  | Brake pedal input as used by the simulation <0;1>. Accounts for the press attack curve for digital inputs. Does not contain retarder, parking or engine brake.                                     |
| `truck.brake.motor`             | bool   | Whether engine braking is enabled.                                                                                                                                                                 |
| `truck.brake.parking`           | bool   | Whether the parking brake is set.                                                                                                                                                                  |
| `truck.brake.retarder`          | u32    | Current level of the retarder. 0 is disabled, maximum is specified by `truck.retarder.steps`.                                                                                                      |
| `truck.retarder.steps`          | u32    | Number of steps in the retarder. Set to zero if retarder is not mounted to the truck.                                                                                                              |
| `controls.shifter.type`         | string | Either `arcade`, `automatic`, `manual`, or `hshifter`.                                                                                                                                             |
| `hshifter.selector.count`       | u32    | Number of selectors (e.g. range/splitter toggles).                                                                                                                                                 |
| `hshifter.slot.handle.position` | u32[]  | Position of h-shifter handle.                                                                                                                                                                      |
| `hshifter.slot.selectors`       | u32[]  | Bitmask of required on/off state of selectors.                                                                                                                                                     |
| `hshifter.slot.gear`            | s32[]  | Gear selected when requirements for this h-shifter slot (corresponding handle position and selectors) are met. The first matching slot dictates the gear. If no gear is found, neutral is assumed. |
| `truck.hshifter.select`         | u32[]  | Enabled state of range/splitter selector toggles. Mapping between the range/splitter functionality and selector index is described by HSHIFTER configuration.                                      |
| `truck.hshifter.slot`           | u32    | Gearbox slot the h-shifter handle is currently in. 0 means that no slot is selected.                                                                                                               |
| `truck.input.clutch`            | float  | Clutch received from input <0;1>.                                                                                                                                                                  |
| `truck.effective.clutch`        | float  | Clutch pedal input as used by the simulation <0;1>. Accounts for the automatic shifting or interpolation of player input.                                                                          |
| `truck.differential_lock`       | bool   | Whether the differential lock is enabled.                                                                                                                                                          |

#### Game performance

| Key                            | Type  | Description                                                                         |
|--------------------------------|-------|-------------------------------------------------------------------------------------|
| `frame.fps`                    | float | Instantaneous frames per second computed from `frame.render_time`.                  |
| `frame.fps_filtered`           | float | Low-pass filtered version of `frame.fps`.                                           |
| `frame.render_time`            | u64   | Time controlling the visualization (microseconds).                                  |
| `frame.simulation_time`        | u64   | Time controlling the physical simulation, updating even when paused (microseconds). |
| `frame.paused_simulation_time` | u64   | Time controlling the physical simulation, stopped when paused (microseconds).       |

#### In-game time

| Key                            | Type  | Description                                                                                                                                                 |
|--------------------------------|-------|-------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `frame.paused`                 | bool  | True when the game is paused or not running.                                                                                                                |
| `game.time`                    | u32   | Absolute in-game time, represented in number of in-game minutes since beginning (i.e. 00:00) of the first in-game day.                                      |
| `rest.stop`                    | s32   | Time until next rest stop, represented in in-game minutes.                                                                                                  |
| `multiplayer.time.offset`      | s32   | Offset from `game.time` simulated in the local economy to the `game.time` of the Convoy multiplayer server in in-game minutes.                              |
| `local.scale`                  | float | Scale applied to distance and time to compensate for the scale of the map (e.g. 1s of real time corresponds to local_scale seconds of simulated game time). |

#### Job information

| Key                                   | Type   | Description                                                                                                                        |
|---------------------------------------|--------|------------------------------------------------------------------------------------------------------------------------------------|
| `job.cargo`                           | string | Name of the cargo for display purposes.                                                                                            |
| `job.cargo.id`                        | string | ID of the cargo for internal use by code.                                                                                          |
| `job.cargo.damage`                    | float  | The total damage of the cargo in range 0.0 to 1.0.                                                                                 |
| `job.cargo.loaded`                    | bool   | Whether the cargo is loaded on the trailer. For non-cargo market this is always true.                                              |
| `job.cargo.mass`                      | float  | Mass of the cargo in kilograms.                                                                                                    |
| `job.cargo.unit.count`                | u32    | How many units of the cargo the job consist of.                                                                                    |
| `job.cargo.unit.mass`                 | float  | Mass of the single unit of the cargo in kilograms.                                                                                 |
| `job.[source/destination].city`       | string | Name of the destination city for display purposes.                                                                                 |
| `job.[source/destination].city.id`    | string | ID of the source/destination city for internal use by code.                                                                        |
| `job.[source/destination].company`    | string | Name of the destination company for display purposes.                                                                              |
| `job.[source/destination].company.id` | string | ID of the destination company for internal use by code.                                                                            |
| `job.delivery.time`                   | u32    | Absolute in-game time in minutes of end of job delivery window.                                                                    |
| `job.income`                          | u64    | Reward in internal game-specific currency.                                                                                         |
| `job.job.market`                      | string | The job market this job is from. One of `cargo_market`, `quick_job`, `freight_market`, `external_contracts`, or `external_market`. |
| `job.is.special.job`                  | bool   | Flag indicating that the job is special transport job.                                                                             |
| `job.planned_distance.km`             | u32    | Planned job distance in simulated kilometers. Does not include ferry distance.                                                     |

#### Truck and trailer identification

| Key                                          | Type   | Description                                                                               |
|----------------------------------------------|--------|-------------------------------------------------------------------------------------------|
| `truck.name`                                 | string | Localized name of the truck.                                                              |
| `trailer.0.body.type`                        | string | Localized name of the trailer body type. Only reported for the first trailer.             |
| `trailer.0.chain.type`                       | string | Name of trailer chain type for internal use by code. Only reported for the first trailer. |
| `[truck/trailer.#].id`                       | string | ID of the truck or trailer for internal use by code.                                      |
| `[truck/trailer.#].brand`                    | string | Localized brand of the truck or trailer.                                                  |
| `[truck/trailer.#].brand_id`                 | string | ID of the truck or trailer brand for internal use by code.                                |
| `[truck/trailer.#].license.plate`            | string | License plate of the truck or trailer.                                                    |
| `[truck/trailer.#].license.plate.country`    | string | Localized name of the country in which the truck or trailer is registered.                |
| `[truck/trailer.#].license.plate.country.id` | string | ID of the country in which the truck or trailer is registered for internal use by code.   |
| `trailer.#.cargo.accessory.id`               | string | Name of cargo accessory for internal use by code.                                         |

#### Positioning

| Key                                            | Type       | Description                                                                                               |
|------------------------------------------------|------------|-----------------------------------------------------------------------------------------------------------|
| `[truck/trailer.#].world.placement`            | dplacement | Represents world space position and orientation of the truck or trailer.                                  |
| `[truck.local/trailer.#].velocity.linear`      | fvector    | Represents vehicle space linear velocity of the truck or trailer measured in meters per second.           |
| `[truck.local/trailer.#].velocity.angular`     | fvector    | Represents vehicle space angular velocity of the truck or trailer measured in rotations per second.       |
| `[truck.local/trailer.#].acceleration.linear`  | fvector    | Represents vehicle space linear acceleration of the truck or trailer measured in meters per second^2.     |
| `[truck.local/trailer.#].acceleration.angular` | fvector    | Represents vehicle space angular acceleration of the truck or trailer measured in rotations per second^2. |
| `[truck/trailer.#].hook.position`              | fvector    | Represents the position of the trailer connection hook in vehicle space.                                  |
| `trailer.#.connected`                          | bool       | Whether the (indexed) trailer is connected to the truck or the preceding trailer.                         |
| `truck.cabin.position`                         | fvector    | Represents the default position of the cabin in the vehicle space, around which the cabin rotates.        |
| `truck.cabin.offset`                           | fplacement | Represents vehicle space position and orientation delta of the cabin from its default position.           |
| `truck.cabin.velocity.angular`                 | fvector    | Represents cabin space angular velocity of the cabin measured in rotations per second.                    |
| `truck.cabin.acceleration.angular`             | fvector    | Represents cabin space angular acceleration of the cabin measured in rotations per second^2.              |
| `truck.head.position`                          | fvector    | Represents the default position of the head in the cabin space.                                           |
| `truck.head.offset`                            | fplacement | Represents a cabin space position and orientation delta of the driver head from its default position.     |

Note the inconsistency that the `.local` path element for velocity and
acceleration is present for trucks but not for trailers; that's SCS's
derp.

#### Powertrain-related

| Key                        | Type    | Description                                                                                |
|----------------------------|---------|--------------------------------------------------------------------------------------------|
| `truck.electric.enabled`   | bool    | Whether electronics are enabled.                                                           |
| `truck.engine.enabled`     | bool    | Whether the engine is enabled.                                                             |
| `truck.engine.rpm`         | float   | RPM of the engine.                                                                         |
| `truck.rpm.limit`          | float   | Maximum engine RPM.                                                                        |
| `truck.engine.gear`        | s32     | Gear currently selected in the engine/gearbox. >0 = drive, 0 = neutral, <0 = reverse.      |
| `truck.displayed.gear`     | s32     | Gear currently displayed on the dashboard. >0 = drive, 0 = neutral, <0 = reverse.          |
| `truck.gears.forward`      | u32     | Number of forward gears on an undamaged truck.                                             |
| `truck.forward.ratio`      | float[] | Forward transmission ratios.                                                               |
| `truck.gears.reverse`      | u32     | Number of reverse gears on an undamaged truck.                                             |
| `truck.reverse.ratio`      | float[] | Reverse transmission ratios.                                                               |
| `truck.differential.ratio` | float   | Differential ratio of the truck.                                                           |
| `truck.speed`              | float   | Speedometer speed in meters per second. Uses negative value to represent reverse movement. |

#### Wheels and axles

| Key                                             | Type      | Description                                                                                                      |
|-------------------------------------------------|-----------|------------------------------------------------------------------------------------------------------------------|
| `[truck/trailer.#].wheels.count`                | u32       | Number of wheels on the truck or trailer.                                                                        |
| `[truck/trailer.#].wheel.position`              | fvector[] | Position of the wheel in the vehicle space.                                                                      |
| `[truck/trailer.#].wheel.powered`               | bool[]    | Whether the wheel is powered.                                                                                    |
| `[truck/trailer.#].wheel.simulated`             | bool[]    | Whether the wheel is physically simulated.                                                                       |
| `[truck/trailer.#].wheel.radius`                | float[]   | Wheel radius.                                                                                                    |
| `[truck/trailer.#].wheel.on_ground`             | bool[]    | Whether the wheel is in contact with the ground.                                                                 |
| `[truck/trailer.#].wheel.substance`             | u32[]     | Index of the substance below the wheel.                                                                          |
| `substances.id`                                 | string[]  | In-game identifiers of substance types.                                                                          |
| `[truck/trailer.#].wheel.angular_velocity`      | float[]   | Angular velocity of the wheel in rotations per second, forward positive.                                         |
| `[truck/trailer.#].wheel.rotation`              | float[]   | Rolling rotation of the wheel in rotations in <0.0,1.0) range, forward increasing.                               |
| `[truck/trailer.#].wheel.suspension.deflection` | float[]   | Vertical displacement of the wheel from its axis in meters.                                                      |
| `[truck/trailer.#].wheel.steerable`             | bool[]    | Whether the wheel is physically steerable.                                                                       |
| `[truck/trailer.#].wheel.steering`              | float[]   | Steering rotation of the wheel in rotations. Ranges from -0.25 for 90 degrees right to 0.25 for 90 degrees left. |
| `trailer.#.wheel.liftable`                      | bool[]    | Whether the wheel is liftable.                                                                                   |
| `[truck/trailer.#].wheel.lift`                  | float[]   | Lift state of the wheel in <0,1> range. 0 means non-lifted or non-liftable, 1 means fully lifted.                |
| `[truck/trailer.#].wheel.lift.offset`           | float[]   | Vertical displacement of the wheel axle from its normal position in meters as result of lifting.                 |
| `[truck/truck.trailer].lift_axle`               | bool      | Whether the lift-axle control for the truck or trailer is set to the lifted state.                               |
| `[truck/truck.trailer].lift_axle.indicator`     | bool      | Whether the lift-axle indicator for the truck or trailer is lit.                                                 |

#### Resources

| Key                                             | Type  | Description                                                                                                                               |
|-------------------------------------------------|-------|-------------------------------------------------------------------------------------------------------------------------------------------|
| `truck.fuel.amount`                             | float | Amount of fuel in liters.                                                                                                                 |
| `truck.fuel.capacity`                           | float | Fuel tank capacity in liters.                                                                                                             |
| `truck.fuel.consumption.average`                | float | Average fuel consumption in liters/km.                                                                                                    |
| `truck.fuel.range`                              | float | Estimated range of truck with current amount of fuel in km.                                                                               |
| `truck.fuel.warning`                            | bool  | Whether the low-fuel warning indicator is lit.                                                                                            |
| `truck.fuel.warning.factor`                     | float | Fraction of fuel capacity below which the low-fuel warning indicator is lit.                                                              |
| `truck.adblue`                                  | float | Amount of AdBlue in liters.                                                                                                               |
| `truck.adblue.capacity`                         | float | AdBlue tank capacity in liters.                                                                                                           |
| `truck.adblue.consumption.average`              | float | Average AdBlue consumption in liters/km. Defined in the API but doesn't seem to be implemented in ETS2 1.18. TruckTel requests it anyway. |
| `truck.adblue.warning`                          | bool  | Whether the low-AdBlue warning indicator is lit.                                                                                          |
| `truck.adblue.warning.factor`                   | float | Fraction of AdBlue capacity below which the low-fuel warning indicator is lit.                                                            |
| `truck.oil.pressure`                            | float | Oil pressure in psi.                                                                                                                      |
| `truck.oil.pressure.warning`                    | bool  | Whether the oil-pressure warning indicator is lit.                                                                                        |
| `truck.oil.pressure.warning.threshold`*         | float | Oil pressure in psi below which the oil-pressure warning indicator is lit.                                                                |
| `truck.oil.temperature`                         | float | Oil temperature in degrees Celsius.                                                                                                       |
| `truck.water.temperature`                       | float | Coolant temperature in degrees Celsius.                                                                                                   |
| `truck.water.temperature.warning`               | bool  | Whether the coolant-temperature warning indicator is lit.                                                                                 |
| `truck.water.temperature.warning.threshold`*    | float | Coolant temperature above which the coolant-temperature warning indicator is lit.                                                         |
| `truck.brake.air.pressure`                      | float | Pressure in the brake air tank in psi.                                                                                                    |
| `truck.brake.air.pressure.warning`              | bool  | Whether the brake pressure warning indicator is lit.                                                                                      |
| `truck.brake.air.pressure.warning.threshold`*   | float | Brake air pressure below which the brake pressure warning indicator is lit.                                                               |
| `truck.brake.air.pressure.emergency`            | bool  | Whether emergency brakes are active as a result of low air pressure.                                                                      |
| `truck.brake.air.pressure.emergency.threshold`* | float | Brake air pressure below which the emergency brakes activate.                                                                             |
| `truck.brake.temperature`                       | float | Temperature of the brakes in degrees Celsius. Approximated for entire truck, not at the wheel level.                                      |
| `truck.battery.voltage`                         | float | Battery voltage in volts.                                                                                                                 |
| `truck.battery.voltage.warning`                 | bool  | Whether the battery warning indicator is lit.                                                                                             |
| `truck.battery.voltage.warning.threshold`*      | float | Battery voltage below which the battery warning indicator is lit.                                                                         |

Asterisks: TruckTel adds `.threshold` to the configuration attribute name to
avoid conflict between configuration and channel data.

#### Damage and wear

| Key                              | Type  | Description                                     |
|----------------------------------|-------|-------------------------------------------------|
| `truck.odometer`                 | float | Odometer of the truck in km.                    |
| `truck.wear.engine`              | float | Engine wear in <0.0, 1.0> range.                |
| `truck.wear.transmission`        | float | Transmission wear in <0.0, 1.0> range.          |
| `[truck/trailer.#].wear.chassis` | float | Truck/trailer chassis wear in <0.0, 1.0> range. |
| `[truck/trailer.#].wear.wheels`  | float | Average tire wear in <0.0, 1.0> range.          |
| `truck.wear.cabin`               | float | Cabin wear in <0.0, 1.0> range.                 |
| `trailer.#.wear.body`            | float | Trailer body wear in <0.0, 1.0> range.          |
| `trailer.#.cargo.damage`         | float | Cargo damage in <0.0, 1.0> range.               |

#### Lights and utilities

| Key                            | Type  | Description                                                                         |
|--------------------------------|-------|-------------------------------------------------------------------------------------|
| `truck.wipers`                 | bool  | State of the wiper switch.                                                          |
| `truck.light.parking`          | bool  | State of the parking lights.                                                        |
| `truck.light.beam.low`         | bool  | State of the low-beam lights.                                                       |
| `truck.light.beam.high`        | bool  | State of the high-beam lights.                                                      |
| `truck.light.aux.front`        | u32   | State of the front auxiliary lights. 0 = off, 1 = dimmed, 2 = full.                 |
| `truck.light.aux.roof`         | u32   | State of the roof-mounted auxiliary lights. 0 = off, 1 = dimmed, 2 = full.          |
| `truck.light.beacon`           | bool  | State of the beacon lights.                                                         |
| `truck.[l/r]blinker`           | bool  | State of the left/right blinker switch.                                             |
| `truck.hazard.warning`         | bool  | State of the hazard-lights switch.                                                  |
| `truck.dashboard.backlight`    | float | Intensity of the dashboard backlight as factor <0;1>.                               |
| `truck.light.[l/r]blinker`     | bool  | Actual state of the left/right blinker.                                             |
| `truck.light.brake`            | bool  | Actual state of the brake lights.                                                   |
| `truck.light.reverse`          | bool  | Actual state of the reverse lights.                                                 |
| `truck.navigation.distance`    | float | The value of truck's navigation distance in meters.                                 |
| `truck.navigation.time`        | float | The value of truck's navigation estimated time of arrival in second.                |
| `truck.navigation.speed.limit` | float | The value of truck's navigation speed limit in m/s.                                 |
| `truck.cruise_control`         | float | Speed selected for the cruise control in m/s. Zero when cruise control is disabled. |

### Events

#### Game pause/unpause

Generated by TruckTel in response to the `SCS_TELEMETRY_EVENT_paused` and
`SCS_TELEMETRY_EVENT_started` events.

| Key | Type   | Description                  |
|-----|--------|------------------------------|
| `_` | string | Event ID: `game.paused` or `game.started`. |

#### Job delivered

Event called when job is delivered.

| Key              | Type   | Description                                                                       |
|------------------|--------|-----------------------------------------------------------------------------------|
| `_`              | string | Event ID: `job.delivered`.                                                        |
| `revenue`        | s64    | The job revenue in native game currency.                                          |
| `earned.xp`      | s32    | How much XP player received for the job.                                          |
| `cargo.damage`   | float  | Total cargo damage, range <0.0, 1.0>.                                             |
| `distance.km`    | float  | The real distance in km on the job.                                               |
| `delivery.time`  | u32    | Total time spend on the job in game minutes.                                      |
| `auto.park.used` | bool   | Whether auto-parking was used on this job.                                        |
| `auto.load.used` | bool   | Whether auto-loading was used on this job. Always true for non-cargo-market jobs. |

#### Job cancelled

Event called when job is cancelled.

| Key              | Type   | Description                                                           |
|------------------|--------|-----------------------------------------------------------------------|
| `_`              | string | Event ID: `job.cancelled`.                                            |
| `cancel.penalty` | s64    | The penalty for cancelling the job in native game currency. Can be 0. |

#### Player fined

Event called when player gets fined.

| Key            | Type   | Description                                  |
|----------------|--------|----------------------------------------------|
| `_`            | string | Event ID: `player.fined`.                    |
| `fine.offence` | string | Fine offense type.                           |
| `fine.amount`  | s64    | Fine offense amount in native game currency. |

The offense types are:

 - `crash`
 - `avoid_sleeping`
 - `wrong_way`
 - `speeding_camera`
 - `no_lights`
 - `red_signal`
 - `speeding`
 - `avoid_weighing`
 - `illegal_trailer`
 - `avoid_inspection`
 - `illegal_border_crossing`
 - `hard_shoulder_violation`
 - `damaged_vehicle_usage`
 - `generic`

#### Tollgate paid

Event called when player pays for a tollgate.

| Key          | Type   | Description                                                          |
|--------------|--------|----------------------------------------------------------------------|
| `_`          | string | Event ID: `player.tollgate.paid`.                                    |
| `pay.amount` | s64    | How much player was charged for this action in native game currency. |

#### Ferry or train used

Event called when player uses a ferry or train.

| Key           | Type   | Description                                                          |
|---------------|--------|----------------------------------------------------------------------|
| `_`           | string | Event ID: `player.use.ferry` or `player.use.train`.                  |
| `pay.amount`  | s64    | How much player was charged for this action in native game currency. |
| `source.id`   | string | The ID of the transportation source.                                 |
| `source.name` | string | The name of the transportation source.                               |
| `target.id`   | string | The ID of the transportation target.                                 |
| `target.name` | string | The name of the transportation target.                               |

### Semantical inputs

Since there's no game documentation and I haven't tried the vast majority of
these, most descriptions are empty. For the inputs without description, the
type is a guess based on the mix expression in the default game configuration,
and it's unknown if the input is functional at all.

Trying out inputs and have descriptions to share? Pull requests welcome!

| Input ID       | Type   | Description                                 |
|----------------|--------|---------------------------------------------|
| `j_left`       | binary |                                             |
| `j_right`      | binary |                                             |
| `j_up`         | binary |                                             |
| `j_down`       | binary |                                             |
| `selectfcs`    | binary |                                             |
| `back`         | binary |                                             |
| `skip`         | binary |                                             |
| `scrol_up`     | binary |                                             |
| `scrol_dwn`    | binary |                                             |
| `mapzoom_in`   | binary |                                             |
| `mapzoom_out`  | binary |                                             |
| `trs_zoom_in`  | binary |                                             |
| `trs_zoom_out` | binary |                                             |
| `joy_nav_prv`  | binary |                                             |
| `joy_nav_nxt`  | binary |                                             |
| `joy_sec_prv`  | binary |                                             |
| `joy_sec_nxt`  | binary |                                             |
| `scroll_j_x`   | float  |                                             |
| `scroll_j_y`   | float  |                                             |
| `shortcut_1`   | binary |                                             |
| `shortcut_1h`  | binary |                                             |
| `shortcut_2`   | binary |                                             |
| `shortcut_2h`  | binary |                                             |
| `shortcut_3`   | binary |                                             |
| `shortcut_3h`  | binary |                                             |
| `shortcut_4`   | binary |                                             |
| `shortcut_4h`  | binary |                                             |
| `pause`        | binary |                                             |
| `screenshot`   | binary |                                             |
| `cam1`         | binary |                                             |
| `cam2`         | binary |                                             |
| `cam3`         | binary |                                             |
| `cam4`         | binary |                                             |
| `cam5`         | binary |                                             |
| `cam6`         | binary |                                             |
| `cam7`         | binary |                                             |
| `cam8`         | binary |                                             |
| `camcycle`     | binary |                                             |
| `camreset`     | binary |                                             |
| `camrotate`    | binary |                                             |
| `camzoomin`    | binary |                                             |
| `camzoomout`   | binary |                                             |
| `camzoom`      | binary |                                             |
| `camfwd`       | binary |                                             |
| `camback`      | binary |                                             |
| `camleft`      | binary |                                             |
| `camright`     | binary |                                             |
| `camup`        | binary |                                             |
| `camdown`      | binary |                                             |
| `lookleft`     | binary |                                             |
| `lookright`    | binary |                                             |
| `camlr`        | float  | Controls camera yaw *rate*, positive right. |
| `camud`        | float  |                                             |
| `j_cam_lk_lr`  | float  |                                             |
| `j_cam_lk_ud`  | float  |                                             |
| `j_cam_mv_lr`  | float  |                                             |
| `j_cam_mv_ud`  | float  |                                             |
| `j_trzoom_in`  | binary |                                             |
| `j_trzoom_out` | binary |                                             |
| `j_tr_cam_res` | binary |                                             |
| `j_tr_cam_swi` | binary |                                             |
| `j_tr_lights`  | binary |                                             |
| `j_tr_fullsc`  | binary |                                             |
| `j_tr_att_tra` | binary |                                             |
| `j_mappan_x`   | float  |                                             |
| `j_mappan_y`   | float  |                                             |
| `j_mapzom_in`  | binary |                                             |
| `j_mapzom_out` | binary |                                             |
| `lookpos1`     | binary |                                             |
| `lookpos2`     | binary |                                             |
| `lookpos3`     | binary |                                             |
| `lookpos4`     | binary |                                             |
| `lookpos5`     | binary |                                             |
| `lookpos6`     | binary |                                             |
| `lookpos7`     | binary |                                             |
| `lookpos8`     | binary |                                             |
| `lookpos9`     | binary |                                             |
| `looksteer`    | binary |                                             |
| `lookblink`    | binary |                                             |
| `activate`     | binary |                                             |
| `menu`         | binary |                                             |
| `engine`       | binary |                                             |
| `engineelect`  | binary |                                             |
| `ignitionoff`  | binary |                                             |
| `ignitionon`   | binary |                                             |
| `ignitionstrt` | binary |                                             |
| `attach`       | binary |                                             |
| `frontsuspup`  | binary |                                             |
| `frontsuspdwn` | binary |                                             |
| `rearsuspup`   | binary |                                             |
| `rearsuspdwn`  | binary |                                             |
| `suspreset`    | binary |                                             |
| `horn`         | binary |                                             |
| `airhorn`      | binary |                                             |
| `lighthorn`    | binary |                                             |
| `beacon`       | binary |                                             |
| `motorbrake`   | binary |                                             |
| `engbraketog`  | binary |                                             |
| `engbrakeup`   | binary |                                             |
| `engbrakedwn`  | binary |                                             |
| `trailerbrake` | binary |                                             |
| `retarderup`   | binary |                                             |
| `retarderdown` | binary |                                             |
| `retarder0`    | binary |                                             |
| `retarder1`    | binary |                                             |
| `retarder2`    | binary |                                             |
| `retarder3`    | binary |                                             |
| `retarder4`    | binary |                                             |
| `retarder5`    | binary |                                             |
| `liftaxle`     | binary |                                             |
| `liftaxlet`    | binary |                                             |
| `slideaxlefwd` | binary |                                             |
| `slideaxlebwd` | binary |                                             |
| `slideaxleman` | binary |                                             |
| `trlrsuspup`   | binary |                                             |
| `trlrsuspdwn`  | binary |                                             |
| `diflock`      | binary |                                             |
| `rwinopen`     | binary |                                             |
| `rwinclose`    | binary |                                             |
| `lwinopen`     | binary |                                             |
| `lwinclose`    | binary |                                             |
| `engbrakeauto` | binary |                                             |
| `retarderauto` | binary |                                             |
| `embrake`      | binary |                                             |
| `laneassmode`  | binary |                                             |
| `tranpwrmode`  | binary |                                             |
| `parkingbrake` | binary |                                             |
| `handbrake`    | binary |                                             |
| `wipers`       | binary |                                             |
| `wipersback`   | binary |                                             |
| `wipers0`      | binary |                                             |
| `wipers1`      | binary |                                             |
| `wipers2`      | binary |                                             |
| `wipers3`      | binary |                                             |
| `wipers4`      | binary |                                             |
| `cruiectrl`    | binary |                                             |
| `cruiectrlinc` | binary |                                             |
| `cruiectrldec` | binary |                                             |
| `cruiectrlres` | binary |                                             |
| `accmode`      | binary |                                             |
| `laneassist`   | binary |                                             |
| `light`        | binary |                                             |
| `lightoff`     | binary |                                             |
| `lightpark`    | binary |                                             |
| `lighton`      | binary |                                             |
| `hblight`      | binary |                                             |
| `lblinker`     | binary | Left blinker toggle button.                 |
| `lblinkerh`    | binary | Left blinker switch.                        |
| `rblinker`     | binary | Right blinker toggle button.                |
| `rblinkerh`    | binary | Right blinker switch.                       |
| `flasher4way`  | binary |                                             |
| `showmirrors`  | binary |                                             |
| `showhud`      | binary |                                             |
| `navmap`       | binary |                                             |
| `photo_mode`   | binary |                                             |
| `quicksave`    | binary |                                             |
| `quickload`    | binary |                                             |
| `acaquickr`    | binary |                                             |
| `radio`        | binary |                                             |
| `radionext`    | binary |                                             |
| `radioprev`    | binary |                                             |
| `radioup`      | binary |                                             |
| `radiodown`    | binary |                                             |
| `display`      | binary |                                             |
| `quickpark`    | binary |                                             |
| `dashmapzoom`  | binary |                                             |
| `tripreset`    | binary |                                             |
| `sb_activate`  | binary |                                             |
| `sb_swap`      | binary |                                             |
| `infotainment` | binary |                                             |
| `mapcenter`    | binary |                                             |
| `photores`     | binary |                                             |
| `photomove`    | binary |                                             |
| `phototake`    | binary |                                             |
| `photofwd`     | binary |                                             |
| `photobwd`     | binary |                                             |
| `photoleft`    | binary |                                             |
| `photoright`   | binary |                                             |
| `photoup`      | binary |                                             |
| `photodown`    | binary |                                             |
| `photorolll`   | binary |                                             |
| `photorollr`   | binary |                                             |
| `photosman`    | binary |                                             |
| `photo_opts`   | binary |                                             |
| `photosnap`    | binary |                                             |
| `photo_hctrl`  | binary |                                             |
| `photonames`   | binary |                                             |
| `photozoomout` | binary |                                             |
| `photozoomin`  | binary |                                             |
| `phot_z_j_out` | binary |                                             |
| `phot_z_j_in`  | binary |                                             |
| `album_pgup`   | binary |                                             |
| `album_pgdn`   | binary |                                             |
| `album_itup`   | binary |                                             |
| `album_itdn`   | binary |                                             |
| `album_itlf`   | binary |                                             |
| `album_itrg`   | binary |                                             |
| `album_ithm`   | binary |                                             |
| `album_iten`   | binary |                                             |
| `album_itac`   | binary |                                             |
| `album_itop`   | binary |                                             |
| `album_itdl`   | binary |                                             |
| `camwalk_for`  | binary |                                             |
| `camwalk_back` | binary |                                             |
| `camwalk_righ` | binary |                                             |
| `camwalk_left` | binary |                                             |
| `camwalk_run`  | binary |                                             |
| `camwalk_jump` | binary |                                             |
| `camwalk_crou` | binary |                                             |
| `camwalk_lr`   | float  |                                             |
| `camwalk_ud`   | float  |                                             |
| `gearup`       | binary |                                             |
| `geardown`     | binary |                                             |
| `gear0`        | binary |                                             |
| `geardrive`    | binary |                                             |
| `gearreverse`  | binary |                                             |
| `gearuphint`   | binary |                                             |
| `geardownhint` | binary |                                             |
| `transemi`     | binary |                                             |
| `drive`        | binary |                                             |
| `reverse`      | binary |                                             |
| `cmirrorsel`   | binary |                                             |
| `fmirrorsel`   | binary |                                             |
| `mirroryawl`   | binary |                                             |
| `mirroryawr`   | binary |                                             |
| `mirrorpitu`   | binary |                                             |
| `mirrorpitl`   | binary |                                             |
| `mirrorreset`  | binary |                                             |
| `quicksel1`    | binary |                                             |
| `quicksel2`    | binary |                                             |
| `quicksel3`    | binary |                                             |
| `quicksel4`    | binary |                                             |
| `quicksel5`    | binary |                                             |
| `quicksel6`    | binary |                                             |
| `quicksel7`    | binary |                                             |
| `quicksel8`    | binary |                                             |
| `mpptt`        | binary |                                             |
| `replayhidec`  | binary |                                             |
| `gearsel1on`   | binary |                                             |
| `gearsel1off`  | binary |                                             |
| `gearsel1tgl`  | binary |                                             |
| `gearsel2on`   | binary |                                             |
| `gearsel2off`  | binary |                                             |
| `gearsel2tgl`  | binary |                                             |
| `gear1`        | binary |                                             |
| `gear2`        | binary |                                             |
| `gear3`        | binary |                                             |
| `gear4`        | binary |                                             |
| `gear5`        | binary |                                             |
| `gear6`        | binary |                                             |
| `gear7`        | binary |                                             |
| `gear8`        | binary |                                             |
| `gear9`        | binary |                                             |
| `gear10`       | binary |                                             |
| `gear11`       | binary |                                             |
| `gear12`       | binary |                                             |
| `gear13`       | binary |                                             |
| `gear14`       | binary |                                             |
| `gear15`       | binary |                                             |
| `gear16`       | binary |                                             |
| `adjuster`     | binary |                                             |
| `advmouse`     | binary |                                             |
| `advetamode`   | binary |                                             |
| `gar_man`      | binary |                                             |
| `advzoomin`    | binary |                                             |
| `advzoomout`   | binary |                                             |
| `advoptions`   | binary |                                             |
| `services`     | binary |                                             |
| `assistact1`   | binary |                                             |
| `assistact2`   | binary |                                             |
| `assistact3`   | binary |                                             |
| `assistact4`   | binary |                                             |
| `assistact5`   | binary |                                             |
| `adj_seats`    | binary |                                             |
| `adj_mirrors`  | binary |                                             |
| `adj_lights`   | binary |                                             |
| `adj_uimirror` | binary |                                             |
| `chat_act`     | binary |                                             |
| `quick_chat`   | binary |                                             |
| `cycl_zoom`    | binary |                                             |
| `name_tags`    | binary |                                             |
| `headreset`    | binary |                                             |
| `menustereo`   | binary |                                             |
