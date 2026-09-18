#include "core/async/JobSystem.h"

#include <condition_variable>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace ars::core::async {

struct Job::State {
    std::shared_ptr<std::atomic_bool> cancelled =
        std::make_shared<std::atomic_bool>(false);
    std::atomic_bool finished{false};
    std::exception_ptr error;
    std::mutex errorMutex;
    std::thread worker;
    std::thread::id workerId{};
};

Job::~Job() {
    if (state_ && state_->worker.joinable()) {
        cancel();
        state_->worker.join();
    }
}

Job& Job::operator=(Job&& other) noexcept {
    if (this == &other) return *this;
    if (state_ && state_->worker.joinable()) {
        cancel();
        state_->worker.join();
    }
    state_ = std::move(other.state_);
    return *this;
}

void Job::cancel() noexcept {
    if (state_) state_->cancelled->store(true, std::memory_order_release);
}

bool Job::ready() const noexcept {
    return !state_ || state_->finished.load(std::memory_order_acquire);
}

bool Job::cancelled() const noexcept {
    return state_ && state_->cancelled->load(std::memory_order_acquire);
}

void Job::wait() {
    if (!state_) return;
    if (std::this_thread::get_id() == state_->workerId) {
        // Задача не должна ждать саму себя: join текущего потока завис бы навсегда.
        // Вместо зависания — явная ошибка, которую можно перехватить и залогировать.
        throw std::logic_error("Job::wait() called from its own worker thread");
    }
    if (state_->worker.joinable()) state_->worker.join();
    std::lock_guard<std::mutex> lock(state_->errorMutex);
    if (state_->error) std::rethrow_exception(state_->error);
}

Job JobSystem::submit(Task task) {
    auto state = std::make_shared<Job::State>();
    const CancellationToken token(state->cancelled);
    state->worker = std::thread([state, task = std::move(task), token]() mutable {
        try {
            if (task) task(token);
        } catch (...) {
            std::lock_guard<std::mutex> lock(state->errorMutex);
            state->error = std::current_exception();
        }
        state->finished.store(true, std::memory_order_release);
    });
    state->workerId = state->worker.get_id();
    return Job(std::move(state));
}

} // namespace ars::core::async
