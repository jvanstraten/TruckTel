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
