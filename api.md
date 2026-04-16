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
then the channel takes precedence..

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

TODO
