#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "logger.h"

/// Abstract base class for worker objects.
class AbstractWorker {
public:
    virtual ~AbstractWorker() = default;

    /// Initialization function. When the associated WorkerThread object is
    /// started, the parent thread is blocked until this function returns or
    /// throws.
    virtual void init() = 0;

    /// Blocking run function that executes the work.
    virtual void run() = 0;
};

/// Manager for running worker objects in a separate thread.
template <class Worker>
class WorkerThread {
private:
    /// The associated worker object.
    std::unique_ptr<AbstractWorker> worker;

    /// Thread handle.
    std::thread thread;

    /// Condition variable used to notify that the server has been initialized.
    std::condition_variable state_cv;

    /// Mutex for init_cv.
    std::mutex state_mutex;

    /// Whether initialization was successful. Guarded by state_mutex and
    /// state_cv.
    std::atomic<bool> init_success = false;

    /// Whether the server thread crashed. Guarded by state_mutex and state_cv.
    std::atomic<bool> worker_crashed = false;

public:
    /// Starts the server.
    void start(std::unique_ptr<AbstractWorker> &&new_worker) {
        worker = std::move(new_worker);
        thread = std::thread([this]() {
            try {
                worker->init();
                {
                    std::unique_lock lock(state_mutex);
                    init_success.store(true);
                }
                state_cv.notify_all();
                worker->run();
            } catch (std::exception &e) {
                Logger::error("fatal error in worker thread: %s", e.what());
                {
                    std::unique_lock lock(state_mutex);
                    worker_crashed.store(true);
                }
                state_cv.notify_all();
            }
        });
        std::unique_lock lk(state_mutex);
        state_cv.wait(lk, [this] {
            return init_success.load() || worker_crashed.load();
        });
    }

    /// Returns a pointer to the worker if the object still exists and the
    /// associated thread is (or might still be) alive.
    Worker *get_worker() {
        if (!worker) return nullptr;
        if (worker_crashed.load()) {
            thread.join();
            worker.reset();
            return nullptr;
        }
        return reinterpret_cast<Worker *>(worker.get());
    }

    /// Joins the worker thread. This should only be called once the worker is
    /// known to halt, otherwise this may deadlock.
    void join() {
        if (!worker) return;
        thread.join();
        worker.reset();
    }

    /// Destructor.
    ~WorkerThread() {
        if (thread.joinable()) thread.join();
    }
};
