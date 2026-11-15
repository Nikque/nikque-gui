// An engine as a child process: commands go to its standard input, its output comes back line by line.
// The lines are read by a thread and handed to the window in batches (one message per batch).
#pragma once

#include "platform.h"

#include <atomic>
#include <mutex>
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
	// writes the text (lines ending with '\n') at once; waits only when the engine left a megabyte unread
	bool send(std::string_view text);
	// moves the lines read so far into `out` (appended)
	void take_lines(std::vector<std::string> &out);
	// "quit" and the end of its input, without waiting: the process stays kept here (and is ended with the GUI) until
	// close(), which does not send "quit" again; exited() says when it has ended
	void quit();
	// the process has ended and its output has been read to its end: take_lines() has all of it (or there is none)
	bool exited() const;
	// "quit", waits up to `wait_ms`, then ends the process if it is still there
	void close(unsigned wait_ms = 2000);

private:
	void read_loop();

#if defined(_WIN32)
	HANDLE process_ = nullptr;
	HANDLE job_ = nullptr;
	HANDLE in_write_ = nullptr;
	HANDLE out_read_ = nullptr;
#else
	int pid_ = -1;
	int in_write_ = -1;
	int out_read_ = -1;
	bool reaped_ = false;
#endif
	std::thread reader_;
	std::atomic<bool> read_ended_{false};  // the reader has read the output to its end (the lines are in lines_)
	std::mutex mutex_;
	std::vector<std::string> lines_;
	std::string partial_;
	NativeWindow notify_ = nullptr;
	unsigned msg_ = 0;
	std::uintptr_t tag_ = 0;
};

}  // namespace ng
