/**
 * Socket class that manages a websocket connection to TruckTel. Automatically
 * reconnects. Example usage:
 *
 * ```ts
 * const game_state = reactive({
 *     current: {
 *         paused: null,
 *         time: null,
 *     },
 *     latest: {
 *         time: null,
 *     },
 * });
 *
 * const socket = new TruckTelSocket("example");
 * socket.current = game_state.current;
 * socket.latest = game_state.latest;
 * socket.throttle = 100;
 * socket.open();
 * ```
 *
 * where "example" has to be defined in your config.yaml, for instance as:
 *
 * ```yaml
 * custom-structures:
 *   example:
 *     paused:
 *       key: frame.paused
 *     time:
 *       key: game.time
 *     engine:
 *       key: truck.engine.enabled
 * ```
 *
 * Tip: for determining whether the game is connected or not, use (for the
 * custom structure above) `game_state.current.paused`:
 *
 *  - `null` => game is not connected
 *  - `true` => game is paused
 *  - `false` => game is running
 */
export class TruckTelSocket {

    /**
     * Maximum update rate in milliseconds. Modify after construction if
     * needed.
     */
    throttle: number = 1000;

    /**
     * Object created with reactive() that will be updated via the socket to
     * hold the *current* value for some game state. When data becomes
     * unavailable in the game, the values of said refs are set to null. Set
     * this to reactive({your structure}) after construction if you want to
     * use this.
     */
    current: any = undefined;

    /**
     * Object created with reactive() that will be updated via the socket to
     * hold the *latest known* value for some game state. When data becomes
     * unavailable in the game, the latest known value received from the socket
     * is retained. Note that refreshing the page will reset this; TruckTel
     * itself does not store latest values. Set this to
     * reactive({your structure}) after construction if you want to use this.
     */
    latest: any = undefined;

    /**
     * Object created with reactive() that will be updated via the socket to
     * hold the *latest unpaused* value for some game state. When data becomes
     * unavailable in the game *but* the game is disconnected or paused, the
     * latest known value received from the socket is retained. For this to
     * work, `paused_key` must be set to a key that reports whether the game
     * is paused and that must be selected via `structure` and `query`. This
     * update type is useful because the game will invalidate many data sources
     * while the game is paused, while the player or your app's logic may not
     * expect them to be, e.g. the current time. Note that refreshing the page
     * will reset the latest state; TruckTel itself does not store latest
     * values. Set this to reactive({your structure}) after construction if you
     * want to use this.
     */
    unpaused: any = undefined;

    /**
     * Key that is used by the selected structure/query to report whether the
     * game is paused. Must be set appropriately when `unpaused` is used.
     */
    paused_key?: string = undefined;

    /**
     * Host and port used during development. When deployed this is not used;
     * the app will be hosted by the same server that hosts the API, so we can
     * use a relative path. Replace this after construction but before open()
     * if you want to use a different port, or if you run the game on a
     * different machine from what you do development on.
     */
    dev_host: string = "localhost:8080";

    /**
     * Whether to print extra debug messages to the console.
     */
    debug: boolean = false;

    /**
     * Constructs a socket for the given structure (single, struct, flat, or
     * something custom defined in config.yaml) and query. Query can be empty
     * to request all data (matching your custom structure, if any). Does not
     * open the socket yet.
     */
    constructor(structure: string, query: string = "") {
        this.structure = structure;
        this.query = query;
        this.debug_id = Date.now();
    }

    /**
     * Open the websocket, if not already open. Note that TruckTelSocket
     * handles automatic reconnection internally.
     */
    open(): void {
        this.ref_count++;
        if (this.debug) {
            console.info(`TruckTelSocket.open() for ${this.debug_id}, ref_count is now ${this.ref_count}`);
        }
        if (this.ref_count > 1) return;
        this.openInternal();
    }

    /**
     * Reopens the websocket, thus updating query, throttle, etc. Ignores
     * reference counts; if there is at least one reference the socket is
     * reopened, if there are no references the socket is closed, and can
     * stay that way, since any future open() will have the same effect as
     * reopen() would have had.
     */
    reopen(): void {
        if (this.debug) {
            console.info(`TruckTelSocket.reopen() for ${this.debug_id}, ref_count is ${this.ref_count}`);
        }
        if (this.ref_count == 0) return;
        this.closeInternal();
        this.openInternal();
    }

    /**
     * Closes the websocket.
     */
    close(): void {
        this.ref_count--;
        if (this.debug) {
            console.info(`TruckTelSocket.close() for ${this.debug_id}, ref_count is now ${this.ref_count}`);
        }
        if (this.ref_count > 0) return;
        this.closeInternal();
    }

    /**
     * Sends a press event for the given binary input.
     */
    pressInput(name: string): void {
        this.send([ "press", name ]);
    }

    /**
     * Sends a hold event for the given binary input.
     */
    holdInput(name: string): void {
        this.send([ "hold", name ]);
    }

    /**
     * Sends a release event for the given binary input.
     */
    releaseInput(name: string): void {
        this.send([ "release", name ]);
    }

    /**
     * Sends a new value for the given float input.
     */
    setInput(name: string, value: number): void {
        this.send([ "set", name, value ]);
    }

    /**
     * Updates non-volatile user-data stored on the server.
     */
    sendUserData(data: any): void {
        this.send({ user: data });
    }

    /**
     * "Structure" field in the TruckTel API request.
     */
    private readonly structure: string;

    /**
     * "Query" field in the TruckTel API request. Can have either period or
     * slash separators.
     */
    private readonly query: string;

    /**
     * Used to uniquely identify the socket in debug messages.
     */
    private readonly debug_id: number;

    /**
     * Reference to the websocket.
     */
    private socket?: WebSocket = undefined;

    /**
     * Reference counter for open/close.
     */
    private ref_count: number = 0;

    /**
     * Whether we're connected to the game.
     */
    private connected: boolean = false;

    /**
     * Whether the game is currently paused. Used and updated only when
     * `paused_key` is set appropriately.
     */
    private paused: boolean = true;

    /**
     * Returns the websocket URL.
     */
    private getSocketUrl(): string {
        let host;
        if (import.meta.env.MODE === "development") {
            host = this.dev_host;
        } else {
            host = window.location.host;
        }
        return `ws://${host}/api/ws/delta/${this.structure}/${
            this.query}?throttle=${this.throttle}`;
    }

    /**
     * Internal logic to open the websocket, ignoring reference counts.
     */
    private openInternal(): void {

        // Don't recreate if the socket already exists.
        if (this.socket !== undefined) return;

        // There's no reason why the development server should open the
        // websocket, since we're targeting a static server anyway.
        if (!import.meta.client) return;

        // Open the websocket.
        let socket_url: string = this.getSocketUrl();
        if (this.debug) {
            console.info("Opening websocket:", socket_url);
        }
        this.socket = new WebSocket(socket_url);
        this.socket.onmessage = (event: MessageEvent<any>): void => {
            const raw_data: any = JSON.parse(event.data);
            if (this.debug) console.info("WebSocket data:", raw_data);
            TruckTelSocket.assignReactive(this.current, raw_data, true);
            TruckTelSocket.assignReactive(this.latest, raw_data, false);
            if (this.paused_key !== undefined) {
                if (this.paused_key in raw_data) {
                    this.paused = raw_data[this.paused_key];
                }
            }
            TruckTelSocket.assignReactive(this.unpaused, raw_data, !this.paused);
        };
        this.socket.onopen = () => {
            if (!this.connected) {
                console.info("Game connected.");
            }
            this.connected = true;
        };
        this.socket.onclose = () => {
            if (this.debug) {
                console.info("Websocket onclose()");
            }
            TruckTelSocket.assignReactive(this.current, null, true);
            TruckTelSocket.assignReactive(this.unpaused, null, true);
            if (this.connected) {
                console.warn("Game disconnected. Trying to reconnect...");
                this.connected = false;
            }
            setTimeout(() => {
                this.socket = undefined;
                this.openInternal();
            }, 1000);
        };
        this.socket.onerror =
            (err) => { console.error("WebSocket error:", err); };
    }

    /**
     * Internal logic to close the websocket, ignoring reference counts.
     */
    private closeInternal(): void {
        if (this.socket === undefined) return;
        if (this.debug) {
            console.info("Closing websocket...");
        }
        this.socket.onclose = () => {};
        this.socket.close();
        this.socket = undefined;
        this.connected = false;
    }

    /**
     * Recursively-called function to update a state object made with
     * reactive(), given delta-coded JSON data from TruckTel. If invalidate is
     * set, values are also set null when data becomes invalid in-game; if not,
     * the last-known value is retained in that case.
     */
    private static assignReactive(state: any, raw_data: any, invalidate: boolean): void {
        if (state == undefined) return;

        // If raw_data is null, recursively invalidate.
        if (invalidate && raw_data === null) {
            for (const key in state) {
                if (isReactive(state[key])) {
                    TruckTelSocket.assignReactive(state[key], null, true);
                } else {
                    state[key] = null;
                }
            }
            return;
        }

        // Call recursively for keys both in raw_data and refs.
        for (const key in raw_data) {
            const raw_item: any = raw_data[key];
            if (key in state) {
                // If the given key maps to another reactive object, apply
                // recursively. Otherwise, assign the value.
                if (isReactive(state[key])) {
                    TruckTelSocket.assignReactive(
                        state[key], raw_item, invalidate
                    );
                } else if (invalidate || raw_item !== null) {
                    state[key] = raw_item;
                }
            }
        }
    }

    /**
     * Sends a message to the websocket.
     */
    private send(message: any): void {
        if (this.socket !== undefined &&
            this.socket.readyState === WebSocket.OPEN) {
            const s = JSON.stringify(message);
            if (this.debug) {
                console.info(`Game input: ${s}`);
            }
            this.socket.send(s);
        } else {
            console.error("Failed to send input; game is not connected");
        }
    }

}
