// std::jthread, std::stop_token and std::stop_source as ng::jthread, ng::stop_token and ng::stop_source: the standard ones
// on Windows; on macOS, whose standard library has them only as experimental, a small one of the same use (a thread
// that is asked to stop and joined when it goes; no stop_callback).
#pragma once

#if defined(_WIN32)

#include <stop_token>
#include <thread>

namespace ng {
using std::jthread;
using std::stop_source;
using std::stop_token;
}  // namespace ng

#else

#include <atomic>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

namespace ng {

class stop_token {
public:
	stop_token() noexcept = default;
	explicit stop_token(std::shared_ptr<std::atomic<bool>> s) noexcept : s_(std::move(s)) {}
	bool stop_requested() const noexcept { return s_ && s_->load(std::memory_order_acquire); }
	bool stop_possible() const noexcept { return static_cast<bool>(s_); }

private:
	std::shared_ptr<std::atomic<bool>> s_;
};

class stop_source {
public:
	stop_source() : s_(std::make_shared<std::atomic<bool>>(false)) {}
	stop_token get_token() const noexcept { return stop_token(s_); }
	bool request_stop() noexcept { return s_ && !s_->exchange(true, std::memory_order_acq_rel); }
	bool stop_requested() const noexcept { return s_ && s_->load(std::memory_order_acquire); }

private:
	std::shared_ptr<std::atomic<bool>> s_;
};

class jthread {
public:
	jthread() noexcept = default;
	template <class F, class... Args, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, jthread>>>
	explicit jthread(F &&f, Args &&...args)
	{
		if constexpr (std::is_invocable_v<std::decay_t<F>, stop_token, std::decay_t<Args>...>)
			t_ = std::thread(std::forward<F>(f), src_.get_token(), std::forward<Args>(args)...);
		else
			t_ = std::thread(std::forward<F>(f), std::forward<Args>(args)...);
	}
	jthread(jthread &&) noexcept = default;
	jthread &operator=(jthread &&o) noexcept
	{
		if (this != &o) {
			end();
			src_ = std::move(o.src_);
			t_ = std::move(o.t_);
		}
		return *this;
	}
	jthread(const jthread &) = delete;
	jthread &operator=(const jthread &) = delete;
	~jthread() { end(); }

	bool joinable() const noexcept { return t_.joinable(); }
	void join() { t_.join(); }
	void detach() { t_.detach(); }
	bool request_stop() noexcept { return src_.request_stop(); }
	stop_token get_stop_token() const noexcept { return src_.get_token(); }
	stop_source get_stop_source() const noexcept { return src_; }

private:
	void end()
	{
		if (t_.joinable()) {
			src_.request_stop();
			t_.join();
		}
	}
	stop_source src_;  // (before the thread: it is made first)
	std::thread t_;
};

}  // namespace ng

#endif
