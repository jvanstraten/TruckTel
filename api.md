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
and channel data; there don't seem to be any, but if there ever are, channels
take precedence.

All of this results in a flat key-value data structure, where the keys have
period separators. But you might also want to think about this in a more
structured way, with nested key-value maps/JSON objects. Unfortunately, this
only half works, because there are many instances where the game will expose
a value for both `x` and `x.y`, for example `truck.adblue` and
`truck.adblue.capacity`. This is a problem because `x` would need to be both
a value and a nested object. Whenever this happens, TruckTel will "replace"
the `x` key with `x._` to make the nesting work. Both this and the flat view
have benefits and drawbacks, so the API supports both forms.

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

## Server endpoints

Your app can get data from TruckTel either via normal HTTP requests (REST) or
via websockets.

### REST

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

### Websocket

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

## Data reference

For detailed documentation,
[download the SCS telemetry API](https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry)
and look at the docstrings in `include/common`. With the background information
above, you should be able to understand those files without knowing anything
about C++.

### Data (channel and configuration)

Notation:

 - In some cases, two similar keys are merged into one in the tables using
   `[x/y]` notation: this means that this part of the key can be either `x` or
   `y`.
 - Trailers are sort of but not really an array in the game; I don't know the
   dev history of the game, but it seems like the notion of multiple trailers
   is an afterthought. You can get information about the trailers via
   `trailer.<key>`, or via `trailer.<index>.<key>`, where `index` is a number
   from 0 to 9. This is represented in the tables below as simply
   `trailer#.<key>`. Note that these numbers are stored as JSON object key
   strings in the structured format, NOT as array indices.

#### Game identification

| Key                | Type    | Description                                                        |
|--------------------|---------|--------------------------------------------------------------------|
| `game.name`        | string  | Localized name of the game.                                        |
| `game.id`          | string  | ID of the game, currently either `eut2` for ETS2 or `ats` for ATS. |
| `game.version`     | version | Game major and minor version.                                      |
| `game.api_version` | version | SCS telemetry API version.                                         |
| `game.install_dir` | string  | Installation directory of the game.                                |

#### Controls

| Key                             | Type   | Description                                                      |
|---------------------------------|--------|------------------------------------------------------------------|
| `controls.shifter.type`         | string | Either `arcade`, `automatic`, `manual`, or `hshifter`.           |
| `hshifter.selector.count`       | u32    | Number of selectors (e.g. range/splitter toggles).               |
| `hshifter.slot.gear`            | s32[]  | Gear selected when requirements for this h-shifter slot are met. |
| `hshifter.slot.handle.position` | u32[]  | Position of h-shifter handle.                                    |
| `hshifter.slot.selectors`       | u32[]  | Bitmask of required on/off state of selectors.                   |
| `truck.input.brake`             | float  |                                                                  |
| `truck.input.clutch`            | float  |                                                                  |
| `truck.input.steering`          | float  |                                                                  |
| `truck.input.throttle`          | float  |                                                                  |
| `truck.effective.brake`         | float  |                                                                  |
| `truck.effective.clutch`        | float  |                                                                  |
| `truck.effective.steering`      | float  |                                                                  |
| `truck.effective.throttle`      | float  |                                                                  |

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

| Key                                   | Type | Description |
|---------------------------------------|------|-------------|
| `job.cargo`                           |      |             |
| `job.cargo.id`                        |      |             |
| `job.cargo.damage`                    |      |             |
| `job.cargo.loaded`                    |      |             |
| `job.cargo.mass`                      |      |             |
| `job.cargo.unit.count`                |      |             |
| `job.cargo.unit.mass`                 |      |             |
| `job.[source/destination].city`       |      |             |
| `job.[source/destination].city.id`    |      |             |
| `job.[source/destination].company`    |      |             |
| `job.[source/destination].company.id` |      |             |
| `job.delivery.time`                   |      |             |
| `job.income`                          |      |             |
| `job.is.special.job`                  |      |             |
| `job.job.market`                      |      |             |
| `job.planned_distance.km`             |      |             |

#### Truck and trailer identification

| Key                                         | Type | Description |
|---------------------------------------------|------|-------------|
| `truck.name`                                |      |             |
| `[truck/trailer#].id`                       |      |             |
| `truck.brand`                               |      |             |
| `truck.brand_id`                            |      |             |
| `[truck/trailer#].license.plate`            |      |             |
| `[truck/trailer#].license.plate.country`    |      |             |
| `[truck/trailer#].license.plate.country.id` |      |             |
| `trailer#.body.type`                        |      |             |
| `trailer#.cargo.accessory.id`               |      |             |
| `trailer#.chain.type`                       |      |             |

#### Positioning

| Key                                | Type | Description |
|------------------------------------|------|-------------|
| `[truck/trailer#].world.placement` |      |             |
| `truck.cabin.acceleration.angular` |      |             |
| `truck.cabin.offset`               |      |             |
| `truck.cabin.position`             |      |             |
| `truck.cabin.velocity.angular`     |      |             |
| `truck.head.offset`                |      |             |
| `truck.head.position`              |      |             |
| `truck.local.acceleration.angular` |      |             |
| `truck.local.acceleration.linear`  |      |             |
| `truck.local.velocity.angular`     |      |             |
| `truck.local.velocity.linear`      |      |             |
| `[truck/trailer#].hook.position`   |      |             |
| `trailer#.acceleration.angular`    |      |             |
| `trailer#.acceleration.linear`     |      |             |
| `trailer#.velocity.angular`        |      |             |
| `trailer#.velocity.linear`         |      |             |
| `trailer#.connected`               |      |             |

#### Powertrain-related

| Key                        | Type | Description |
|----------------------------|------|-------------|
| `truck.electric.enabled`   |      |             |
| `truck.engine.enabled`     |      |             |
| `truck.engine.gear`        |      |             |
| `truck.engine.rpm`         |      |             |
| `truck.rpm.limit`          |      |             |
| `truck.displayed.gear`     |      |             |
| `truck.gears.forward`      |      |             |
| `truck.forward.ratio`      |      |             |
| `truck.gears.reverse`      |      |             |
| `truck.reverse.ratio`      |      |             |
| `truck.differential.ratio` |      |             |
| `truck.differential_lock`  |      |             |
| `truck.speed`              |      |             |

#### Brakes

| Key                                  | Type | Description |
|--------------------------------------|------|-------------|
| `truck.brake.air.pressure`           |      |             |
| `truck.brake.air.pressure.emergency` |      |             |
| `truck.brake.air.pressure.warning`   |      |             |
| `truck.brake.motor`                  |      |             |
| `truck.brake.parking`                |      |             |
| `truck.brake.retarder`               |      |             |
| `truck.retarder.steps`               |      |             |
| `truck.brake.temperature`            |      |             |

#### Wheels and axles

| Key                                            | Type | Description |
|------------------------------------------------|------|-------------|
| `[truck/trailer#].wheels.count`                |      |             |
| `[truck/trailer#].wheel.position`              |      |             |
| `[truck/trailer#].wheel.powered`               |      |             |
| `[truck/trailer#].wheel.simulated`             |      |             |
| `[truck/trailer#].wheel.radius`                |      |             |
| `[truck/trailer#].wheel.on_ground`             |      |             |
| `[truck/trailer#].wheel.substance`             |      |             |
| `substances.id`                                |      |             |
| `[truck/trailer#].wheel.suspension.deflection` |      |             |
| `[truck/trailer#].wheel.steerable`             |      |             |
| `[truck/trailer#].wheel.steering`              |      |             |
| `[truck/trailer#].wheel.rotation`              |      |             |
| `[truck/trailer#].wheel.angular_velocity`      |      |             |
| `trailer#.wheel.liftable`                      |      |             |
| `[truck/trailer#].wheel.lift`                  |      |             |
| `[truck/trailer#].wheel.lift.offset`           |      |             |
| `[truck/truck.trailer].lift_axle`              |      |             |
| `[truck/truck.trailer].lift_axle.indicator`    |      |             |

#### Resources

| Key                               | Type | Description |
|-----------------------------------|------|-------------|
| `truck.fuel.amount`               |      |             |
| `truck.fuel.capacity`             |      |             |
| `truck.fuel.consumption.average`  |      |             |
| `truck.fuel.range`                |      |             |
| `truck.fuel.warning`              |      |             |
| `truck.fuel.warning.factor`       |      |             |
| `truck.adblue`                    |      |             |
| `truck.adblue.capacity`           |      |             |
| `truck.adblue.warning`            |      |             |
| `truck.adblue.warning.factor`     |      |             |
| `truck.oil.pressure`              |      |             |
| `truck.oil.pressure.warning`      |      |             |
| `truck.oil.temperature`           |      |             |
| `truck.water.temperature`         |      |             |
| `truck.water.temperature.warning` |      |             |
| `truck.battery.voltage`           |      |             |
| `truck.battery.voltage.warning`   |      |             |

#### Damage and wear

| Key                             | Type | Description |
|---------------------------------|------|-------------|
| `truck.odometer`                |      |             |
| `truck.wear.cabin`              |      |             |
| `truck.wear.engine`             |      |             |
| `truck.wear.transmission`       |      |             |
| `[truck/trailer#].wear.chassis` |      |             |
| `[truck/trailer#].wear.wheels`  |      |             |
| `trailer#.wear.body`            |      |             |
| `trailer#.cargo.damage`         |      |             |

#### Lights and utilities

| Key                            | Type | Description |
|--------------------------------|------|-------------|
| `truck.wipers`                 |      |             |
| `truck.light.beam.low`         |      |             |
| `truck.light.beam.high`        |      |             |
| `truck.light.beacon`           |      |             |
| `truck.light.aux.front`        |      |             |
| `truck.light.aux.roof`         |      |             |
| `truck.light.brake`            |      |             |
| `truck.light.reverse`          |      |             |
| `truck.hazard.warning`         |      |             |
| `truck.[l/r]blinker`           |      |             |
| `truck.light.[l/r]blinker`     |      |             |
| `truck.light.parking`          |      |             |
| `truck.dashboard.backlight`    |      |             |
| `truck.navigation.distance`    |      |             |
| `truck.navigation.speed.limit` |      |             |
| `truck.navigation.time`        |      |             |
| `truck.cruise_control`         |      |             |

TODO: xref with API docs to check if this is everything and fill out type/description

### Events

TODO
