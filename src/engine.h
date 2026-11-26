// An engine as a child process: commands go to its standard input, its output comes back line by line.
// The lines are read by a thread and handed to the window in batches (one message per batch). The commands are
// written by another thread: an engine that does not read them (busy, or stuck) keeps neither the window nor its close.
#pragma once

#include "platform.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace ng {

class EngineProcess {
public:
	EngineProcess() = default;
	EngineProcess(const EngineProcess &) = delete;
	EngineProcess &operator=(const EngineProcess &) = delete;
	~EngineProcess() { close(); }

	// starts `exe` with `args` in `dir`; `notify` gets message `msg` (wParam = `tag`) when lines arrive
	bool start(const std::wstring &exe, const std::wstring &args, const std::wstring &dir, NativeWindow notify, unsigned msg,
	           std::uintptr_t tag, std::wstring *error);
	bool running() const noexcept;
	// the text (lines ending with '\n') goes to the engine in order, without waiting for it; false when it cannot go:
	// the engine is closed, a write to it has failed (it has ended), or it has left too much unread
	bool send(std::string_view text)
	{
		std::lock_guard lock(write_mutex_);
		if (!writer_.joinable() || input_closing_ || write_failed_ || to_write_.size() + text.size() > kMaxUnwritten) return false;
		to_write_.append(text);
		write_cv_.notify_one();
		return true;
	}
	// moves the lines read so far into `out` (appended)
	void take_lines(std::vector<std::string> &out);
	// "quit" and the end of its input, without waiting: the process stays kept here (and is ended with the GUI) until
	// close(), which does not send "quit" again; exited() says when it has ended
	void quit();
	// the process has ended and its output has been read to its end: take_lines() has all of it (or there is none)
	bool exited() const;
	// the exit code of the process that has ended (nullopt: it runs, it was ended here, or none was started)
	std::optional<int> exit_code() const;
	// "quit", waits up to `wait_ms`, then ends the process if it is still there
	void close(unsigned wait_ms = 2000);

private:
	static constexpr size_t kMaxUnwritten = size_t{64} << 20;
	void read_loop();
	// a line read (without its end): the prompts ">" of the engine around it go, an empty line is not kept
	static void add_line(std::vector<std::string> &batch, const std::string &line)
	{
		size_t a = 0;
		while (a < line.size() && (line[a] == '>' || line[a] == ' ')) ++a;  // the prompt may come before a line
		size_t e = line.size();
		while (e > a && line[e - 1] == '>') --e;  // ... and after one ("Sorting book...done>")
		if (a < e) batch.emplace_back(line.substr(a, e - a));
	}
	// the thread of the commands: writes what send() gave, in order; at the end of the input (quit) it writes what is
	// left, then closes the input of the engine
	void write_loop()
	{
		for (;;) {
			std::string chunk;
			{
				std::unique_lock lock(write_mutex_);
				write_cv_.wait(lock, [this] { return !to_write_.empty() || input_closing_; });
				if (to_write_.empty() || stop_io_) break;
				chunk.swap(to_write_);
			}
			if (!write_all(chunk)) {
				{
					std::lock_guard lock(write_mutex_);
					write_failed_ = true;
					to_write_.clear();
				}
				if (!stop_io_) notify_write_failure();
				break;
			}
		}
		close_input();
		writer_ended_ = true;
	}
	// no command after this: the thread writes what is left, then closes the input
	void end_input()
	{
		std::lock_guard lock(write_mutex_);
		input_closing_ = true;
		write_cv_.notify_one();
	}
	// after the end of the process: the thread ends (a write it waits on fails then) and is joined
	void stop_writer();
	bool write_all(std::string_view text);  // waits until it is written; false when it cannot be
	void close_input();
	void notify_write_failure();

#if defined(_WIN32)
	HANDLE process_ = nullptr;
	HANDLE job_ = nullptr;
	HANDLE in_write_ = nullptr;
	HANDLE out_read_ = nullptr;
	mutable ULONGLONG drain_until_ = 0;
#else
	int pid_ = -1;
	int in_write_ = -1;
	int out_read_ = -1;
	bool reaped_ = false;
	int status_ = -1;  // of waitpid, when reaped
#endif
	std::thread reader_;
	std::thread writer_;
	std::mutex write_mutex_;
	std::condition_variable write_cv_;
	std::string to_write_;        // the commands not written yet
	bool input_closing_ = false;  // quit: nothing is taken any more
	bool write_failed_ = false;
	std::atomic<bool> stop_io_{false};
	mutable std::atomic<bool> drain_output_{false};
	std::atomic<bool> writer_ended_{false};
	std::atomic<bool> read_ended_{false};  // the reader has read the output to its end (the lines are in lines_)
	std::mutex mutex_;
	std::vector<std::string> lines_;
	std::string partial_;
	NativeWindow notify_ = nullptr;
	unsigned msg_ = 0;
	std::uintptr_t tag_ = 0;
};

}  // namespace ng
