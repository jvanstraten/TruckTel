#pragma once

// API URLs have the following forms:
//  - REST: api/rest/<structure>/<query...>
//  - WebSocket: api/ws/<data-type>/<structure>/<query...>[?throttle=<throttle>]
// where <query...> is a bundle of zero or more path elements used to select
// which (group of) key(s) are requested.

/// First path component of URLs that are handled procedurally.
static constexpr auto API_ROOT = "api";

/// Second path component of URLs that are handled as REST queries.
static constexpr auto API_REST = "rest";

/// Second path component of URLs that are handled as websocket queries.
static constexpr auto API_WS = "ws";

//==============================================================================
// Websocket data types
//==============================================================================

/// Websocket "event" data type. Event websockets yield JSON objects that
/// represent (gameplay) events. The event ID is stored in the "_" key. Any
/// named attributes from the SCS telemetry API are encoded using the selected
/// structure, which can be struct or flat (single is not supported).
static constexpr auto API_WS_EVENT = "event";

/// Websocket "data" data type. Data websockets repeatedly send the JSON that
/// would be sent by the equivalent REST query.
static constexpr auto API_WS_DATA = "data";

/// Websocket "delta" data type. Data websockets repeatedly send the JSON that
/// would be sent by the equivalent REST query, but delta-coded for each object:
///  - new or modified object keys are sent normally;
///  - object keys that did not change since the previous message are omitted;
///  - for object keys that were removed since the previous message JSON null
///    is sent.
/// If the above yields an empty object, no message is sent at all.
static constexpr auto API_WS_DELTA = "delta";

//==============================================================================
// Websocket throttling
//==============================================================================

/// Websockets can be artificially throttled using a throttle query string,
/// like this: <path>?throttle=<throttle>. The value must be an unsigned integer
/// that specifies a minimum number of milliseconds between (batches of)
/// websocket messages. For event streams, multiple events may be sent in a
/// single batch (in general, throttling makes little sense for those).
static constexpr auto API_WS_THROTTLE = "throttle";

/// If the throttle parameter is not specified, it defaults to 1000 for data and
/// delta sockets.
static constexpr unsigned long API_WS_DEFAULT_THROTTLE = 1000;

//==============================================================================
// Data structuring
//==============================================================================

/// Send values for all keys of which the query is a prefix. An attempt is made
/// to structure the data naturally for JSON using nested objects. Sometimes
/// this doesn't work though, because the SCS telemetry API has the annoying
/// tendency to define data for "non-leaf" nodes, e.g. it defines both:
///
///  - `job.cargo`: localized name for the cargo type;
///  - `job.cargo.id`: internal identifier for the cargo type.
///
/// This doesn't work for JSON naturally, because the x in
/// `{"job": {"cargo": x}}` would need to be both a string and an object. To
/// solve this, the structure generator implicitly appends a `_` to the
/// conflicting path, e.g. it turns `job.cargo` into `job.cargo._`. Because
/// this is conditioned on the existence of conflicting keys, you unfortunately
/// can't rely on this always happening, so this structure is probably a bit
/// annoying to work with. It saves bandwidth compared to `flat` though by not
/// repeating common key prefixes ad nauseam.
static constexpr auto API_STRUCTURE_STRUCT = "struct";

/// Send values for all keys of which the query is a prefix. No attempt is made
/// to make a hierarchical JSON-y structure from the SCS telemetry API keys that
/// match the query; instead, a single object is sent, of which the keys are the
/// period-separated keys from the SCS telemetry API. This is more consistent
/// than `struct`, but uses a bit more bandwidth.
static constexpr auto API_STRUCTURE_FLAT = "flat";

/// Send only the value of which the key exactly matches the quea single value
/// of which the key exactly matches the query.
static constexpr auto API_STRUCTURE_SINGLE = "single";

//==============================================================================
// TruckTel-defined data
//==============================================================================
// In addition to the channels and configurations defined by the game (see
// common/scssdk_*_channels.h and common/scssdk_*_configs.h), TruckTel generates
// a few additional keys.

/// Configuration key for basic information about the game.
static constexpr auto API_CONFIG_GAME = "game";

/// Localized name of the game, reported by the game at plugin initialization
/// via `init_params->common.game_name`.
static constexpr auto API_CONFIG_GAME_ATTRIBUTE_NAME = "name";

/// Identifier of the game, reported by the game at plugin initialization via
/// `init_params->common.game_name`.
static constexpr auto API_CONFIG_GAME_ATTRIBUTE_ID = "id";

/// Game version, reported by the game at plugin initialization via
/// `init_params->common.game_version`. Reported as an array: [major, minor].
static constexpr auto API_CONFIG_GAME_ATTRIBUTE_VERSION = "version";

/// SCS telemetry API version, reported by the game at plugin initialization via
/// the version parameter. Reported as an array: [major, minor].
static constexpr auto API_CONFIG_GAME_ATTRIBUTE_API_VERSION = "api_version";

/// Installation directory of the game, determined by TruckTel based on the
/// working directory when the plugin is loaded.
static constexpr auto API_CONFIG_GAME_ATTRIBUTE_INSTALL_DIR = "install_dir";

/// Channel for the render time argument passed along with
/// SCS_TELEMETRY_EVENT_frame_start. See
/// scs_telemetry_frame_start_t.render_time.
static constexpr auto API_FRAME_CHANNEL_RENDER_TIME = "frame.render_time";

/// Approximate frames per second based on the previous and the current value
/// of render_time. Floating point number.
static constexpr auto API_FRAME_CHANNEL_FPS = "frame.fps";

/// Approximate frames per second based on the previous and the current value
/// of render_time, with an added lowpass filter. Floating point number.
static constexpr auto API_FRAME_CHANNEL_FPS_FILTERED = "frame.fps_filtered";

/// Filter constant for the FPS lowpass filter.
static constexpr auto API_FPS_FILTER_CONSTANT = 0.1f;

/// Channel for the simulation time argument passed along with
/// SCS_TELEMETRY_EVENT_frame_start. See
/// scs_telemetry_frame_start_t.simulation_time.
static constexpr auto API_FRAME_CHANNEL_SIMULATION_TIME =
    "frame.simulation_time";

/// Channel for the paused simulation time argument passed along with
/// SCS_TELEMETRY_EVENT_frame_start. See
/// scs_telemetry_frame_start_t.paused_simulation_time.
static constexpr auto API_FRAME_CHANNEL_PAUSED_SIMULATION_TIME =
    "frame.paused_simulation_time";

/// Channel reporting whether the game is paused. Tracked by TruckTel based on
/// the start and pause events.
static constexpr auto API_FRAME_CHANNEL_PAUSED = "frame.paused";

/// The SCS API headers don't specify a maximum supported number of wheels for
/// a truck or trailer. This value was determined experimentally from ETS2 1.18.
static constexpr auto API_MAX_WHEELS = 14;

/// The SCS API headers don't specify a maximum supported number of items for
/// SCS_TELEMETRY_TRUCK_CHANNEL_hshifter_selector. This value was determined
/// experimentally from ETS2 1.18. (I don't really understand what this channel
/// does to begin with and don't have an H-shifter to reverse-engineer it.)
static constexpr auto API_MAX_HSHIFTER_SELECTORS = 2;

//==============================================================================
// TruckTel-defined events
//==============================================================================
// In addition to the gameplay events generated by the game (see
// common/scssdk_telemetry_common_gameplay_events.h), TruckTel generates the
// following events in addition.

/// Event generated when the game is unpaused or the simulation is first
/// started. No attributes.
static constexpr auto API_EVENT_STARTED = "game.started";

/// Event generated when the game is paused. No attributes.
static constexpr auto API_EVENT_PAUSED = "game.paused";
