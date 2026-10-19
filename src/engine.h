// An engine as a child process: commands go to its standard input, its output comes back line by line.
// The lines are read by a thread and handed to the window in batches (one message per batch).
#pragma once

#include <windows.h>

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
	bool start(const std::wstring &exe, const std::wstring &args, const std::wstring &dir, HWND notify, UINT msg,
	           WPARAM tag, std::wstring *error);
	bool running() const noexcept { return process_ != nullptr; }
	// whether the process has ended by itself
	bool exited() const;
	// writes the text (lines ending with '\n') at once
	bool send(std::string_view text);
	// moves the lines read so far into `out` (appended)
	void take_lines(std::vector<std::string> &out);
	// "quit", waits up to `wait_ms`, then ends the process if it is still there
	void close(DWORD wait_ms = 2000);
	DWORD pid() const noexcept { return pid_; }

private:
	void read_loop();

	HANDLE process_ = nullptr;
	HANDLE job_ = nullptr;
	HANDLE in_write_ = nullptr;
	HANDLE out_read_ = nullptr;
	DWORD pid_ = 0;
	std::thread reader_;
	std::mutex mutex_;
	std::vector<std::string> lines_;
	std::string partial_;
	HWND notify_ = nullptr;
	UINT msg_ = 0;
	WPARAM tag_ = 0;
};

}  // namespace ng
