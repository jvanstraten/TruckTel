// Socket class that manages a websocket connection to TruckTel. Automatically
// reconnects. Example usage:
//
//     const game_state = {
//         current: {
//             paused: ref(null),
//             time: ref(null),
//         },
//         latest: {
//             time: ref(null),
//         },
//     };
//
//     const socket = new TruckTelSocket("dash");
//     socket.current = game_state.current;
//     socket.latest = game_state.latest;
//     socket.throttle = 100;
//     socket.open();
//
export class TruckTelSocket {

    // Constructs a socket for the given structure (single, struct, flat, or
    // something custom defined in config.yaml) and query. Query can be empty
    // to request all data (matching your custom structure, if any). Does not
    // open the socket yet.
    constructor(structure, query = "") {
        this.structure = structure;
        this.query = query;
        this.#socket = null;
        this.#connected = false;

        // Maximum update rate in milliseconds. Modify after construction if
        // needed.
        this.throttle = 1000;

        // Object with refs that will be updated via the socket to hold the
        // current value for some game state. When data becomes unavailable
        // in the game, the values of said refs are set to null. Add to this
        // or replace it after construction.
        this.current = {};

        // Like current, but values are NOT set to null when they no longer
        // exist in the game. Add to this or replace it after construction.
        this.latest = {};

        // Port used during development. When deployed this is not used; the
        // app will be hosted by the same server that hosts the API, so we
        // can use a relative path. Replace this after construction if needed.
        this.dev_host = "localhost:8080";

        // Whether to print debug messages.
        this.debug = import.meta.env.MODE === "development";
    }

    // Reference to the websocket.
    #socket;

    // Whether we're connected to the game.
    #connected;

    // Recursively-called function to update refs, given delta-coded JSON data
    // from TruckTel. If invalidate is set, refs are also set to null when data
    // becomes invalid in-game.
    static #assignRefs(refs, raw_data, invalidate) {
        // If "value" exists in refs, refs is probably a single Vue ref. Stick
        // the raw data straight into it in that case.
        if ("value" in refs) {
            if (invalidate || raw_data !== null) {
                refs.value = raw_data;
            }
            return;
        }

        // If raw_data is null, recursively invalidate refs.
        if (invalidate && raw_data === null) {
            for (const key in refs) {
                TruckTelSocket.#assignRefs(refs[key], null, invalidate);
            }
            return;
        }

        // Call recursively for keys both in raw_data and refs.
        for (const key in raw_data) {
            const raw_item = raw_data[key];
            if (key in refs) {
                TruckTelSocket.#assignRefs(refs[key], raw_item, invalidate);
            }
        }
    }

    // Returns the websocket URL.
    #getSocketUrl() {
        let host;
        if (import.meta.env.MODE == "development") {
            host = this.dev_host;
        } else {
            host = window.location.host;
        }
        return `ws://${host}/api/ws/delta/${this.structure}/${
            this.query}?throttle=${this.throttle}`;
    }

    // Open the websocket, if not already open.
    open() {
        // Don't reopen if a socket has been opened before and close() has not
        // been called since.
        if (this.#socket !== null) {
            if (this.debug) {
                console.warn("Trying to reopen TruckTel socket; this is no-op");
            }
            return;
        }

        // There's no reason why the development server should open the
        // websocket, since we're targeting a static server anyway.
        if (!import.meta.client) return;

        // Open the websocket.
        let socket_url = this.#getSocketUrl();
        if (this.debug) {
            console.info("Opening websocket:", socket_url);
        }
        this.#socket = new WebSocket(socket_url);
        this.#socket.onmessage = (event) => {
            const raw_data = JSON.parse(event.data);
            if (this.debug) console.info("WebSocket data:", raw_data);
            TruckTelSocket.#assignRefs(this.current, raw_data, true);
            TruckTelSocket.#assignRefs(this.latest, raw_data, false);
        };
        this.#socket.onopen = () => {
            if (!this.#connected) {
                console.info("Game connected.");
            }
            this.#connected = true;
        };
        this.#socket.onclose = () => {
            TruckTelSocket.#assignRefs(this.current, null, true);
            if (this.#connected) {
                console.warn("Game disconnected. Trying to reconnect...");
                this.#connected = false;
            }
            setTimeout(() => {
                this.#socket = null;
                this.open();
            }, 1000);
        };
        this.#socket.onerror =
            (err) => { console.error("WebSocket error:", err); };
    }

    // Closes the websocket.
    close() {
        if (this.#socket === null) return;
        if (this.debug) {
            console.info("Closing websocket...");
        }
        this.#socket.onclose = () => {};
        this.#socket.close();
        this.#socket = null;
        this.#connected = false;
    }

    // Sends a message to the websocket.
    #send(message) {
        if (this.#socket !== null &&
            this.#socket.readyState === WebSocket.OPEN) {
            this.#socket.send(JSON.stringify(message));
        } else {
            console.error("Failed to send input; game is not connected");
        }
    }

    // Sends a press event for the given binary input.
    pressInput(name) {
        this.#send([ "press", name ]);
    }

    // Sends a hold event for the given binary input.
    holdInput(name) {
        this.#send([ "hold", name ]);
    }

    // Sends a release event for the given binary input.
    releaseInput(name) {
        this.#send([ "release", name ]);
    }

    // Sends a new value for the given float input.
    setInput(name, value) {
        this.#send([ "set", name, value ]);
    }
}
