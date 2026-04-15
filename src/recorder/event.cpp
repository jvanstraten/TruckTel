#include "event.h"

#include "logger.h"

EventRecorder::EventRecorder(const std::chrono::system_clock::duration max_age)
    : max_age(max_age) {}

void EventRecorder::push(std::vector<NamedValue> event) {
    std::lock_guard guard(mutex);

    // Perform pruning.
    const auto now = std::chrono::system_clock::now();
    const auto prune_before = now - max_age;
    while (!events.empty() && events.front().timestamp < prune_before) {
        events.pop_front();
    }

    // Push the event.
    Logger::verbose("pushing event %llu", id_counter);
    events.emplace_back(Event{id_counter, now, std::move(event)});

    // Update the ID counter.
    id_counter++;
}

uint64_t EventRecorder::poll_init() {
    std::lock_guard guard(mutex);
    return id_counter;
}

std::vector<std::vector<NamedValue>> EventRecorder::poll(uint64_t &next_id) {
    std::lock_guard guard(mutex);

    // Exit early if there are no new events.
    if (id_counter == next_id) return {};

    // Starting from the newest event, rewind the list until we find the
    // event with next_id or the start of the event list. The event that
    // came before that will already have been reported to this client.
    auto it = events.cend();
    while (it != events.cbegin()) {
        --it;
        if (it->id == next_id) {
            break;
        }
    }

    // Copy all the events that are new for this client into a JSON array.
    std::vector<std::vector<NamedValue>> result;
    for (; it != events.cend(); ++it) {
        result.emplace_back(it->data);
    }

    // Update the ID.
    next_id = id_counter;
    return result;
}
