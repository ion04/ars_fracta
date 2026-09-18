#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

namespace ars::core::async {

/** @brief Shared cancellation flag visible to a background calculation. */
class CancellationToken {
public:
    /** @return true when the owner requested cancellation. */
    bool isCancelled() const noexcept { return state_ && state_->load(); }

private:
    friend class JobSystem;
    explicit CancellationToken(std::shared_ptr<std::atomic_bool> state)
        : state_(std::move(state)) {}
    std::shared_ptr<std::atomic_bool> state_;
};

/**
 * @brief A joinable, exception-preserving asynchronous operation.
 * Handle methods must be called by one owning thread. Destruction and move
 * assignment cancel and join the old task: tasks must cooperate to bound shutdown.
 * A task must not destroy or wait on its own handle. ready() publishes task writes.
 */
class Job {
public:
    /** @brief Create an empty handle (ready, not cancelled). */
    Job() = default;
    Job(Job&&) noexcept = default;
    Job& operator=(Job&& other) noexcept;
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    ~Job();

    /** Request cancellation; the worker observes it cooperatively. */
    void cancel() noexcept;
    /** @return true if the worker has finished (successfully or exceptionally). */
    bool ready() const noexcept;
    /**
     * Wait for completion and rethrow an exception raised by the worker.
     * Throws std::logic_error when invoked from the task's own worker thread
     * (guards against a self-join deadlock instead of hanging).
     */
    void wait();
    /** @return true if cancellation was requested, not proof the task stopped. */
    bool cancelled() const noexcept;

private:
    struct State;
    explicit Job(std::shared_ptr<State> state) : state_(std::move(state)) {}
    friend class JobSystem;
    std::shared_ptr<State> state_;
};

/** @brief Launches independent CPU jobs without touching the UI thread. */
class JobSystem {
public:
    using Task = std::function<void(const CancellationToken&)>;

    /**
     * @brief Start a task immediately on a dedicated worker thread.
     *
     * The task must not call OpenGL or ImGui. Poll `Job::ready()` from the UI
     * thread and call `wait()` only after it reports ready.
     */
    static Job submit(Task task);
};

} // namespace ars::core::async
