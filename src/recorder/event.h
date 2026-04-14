#pragma once

#include <chrono>
#include <cstdint>
#include <list>
#include <mutex>

#include <nlohmann/json.hpp>

#include "json_utils.h"

/// Recorder for a stream of events that should be delivered to clients
/// completely but without duplicates.
class EventRecorder {
private:
    /// Structure for individual events in the queue.
    struct Event {
        /// Unique ID of this event.
        uint64_t id;

        /// Timestamp that this event was generated.
        std::chrono::system_clock::time_point timestamp;

        /// Event attributes.
        std::vector<NamedValue> data;
    };

    /// Event queue. Newer events are inserted at the end of the queue, and old
    /// events are pruned from the start of the queue.
    std::list<Event> events;

    /// Events are stored for at most this amount of time. In order to not drop
    /// events, a client must query the event recorder at least this often.
    /// Pruning only happens when a new event is posted by the SCS API, though.
    const std::chrono::system_clock::duration max_age;

    /// The next event ID.
    uint64_t id_counter = 0;

    /// The mutex that protects all of the above.
    std::mutex mutex;

public:
    /// Constructor. max_age sets the age after which events may be pruned.
    explicit EventRecorder(std::chrono::system_clock::duration max_age);

    /// Pushes an event into the queue. This first prunes old events to avoid
    /// the memory footprint from growing without bound.
    void push(std::vector<NamedValue> event);

    /// Initializes the next_id value for poll() when a client first connects.
    uint64_t poll_init();

    /// Polls for events. The client doing the polling should keep track of the
    /// next_id variable; it is used to avoid sending duplicates. next_id will
    /// be incremented by the number of events returned by the function if and
    /// only if no events were dropped; it will be incremented by N more than
    /// the number of events returned if N events were dropped.
    std::vector<std::vector<NamedValue>> poll(uint64_t &next_id);
};
