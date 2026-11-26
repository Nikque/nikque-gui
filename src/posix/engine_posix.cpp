// src/engine.h on Linux and Android: fork and exec with pipes. The engine reads its commands from its standard input
// and ends at its end ("quit", or the end of the input when the GUI ends without closing it).
#include "../engine.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <initializer_list>

#include "../ini.h"

namespace ng {

namespace {

// the words of a command line: separated by spaces, a part in double quotes is one word (with its spaces)
std::vector<std::string> split_args(const std::string &s)
{
	std::vector<std::string> out;
	std::string cur;
	bool quoted = false, any = false;
	for (const char c : s) {
		if (c == '"') {
			quoted = !quoted;
			any = true;
		} else if (c == ' ' && !quoted) {
			if (any) out.push_back(cur);
			cur.clear();
			any = false;
		} else {
			cur += c;
			any = true;
		}
	}
	if (any) out.push_back(cur);
	return out;
}

void close_all(std::initializer_list<int> fds)
{
	for (int fd : fds)
		if (fd >= 0) ::close(fd);
}

}  // namespace

bool EngineProcess::running() const noexcept { return pid_ > 0; }

bool EngineProcess::start(const std::wstring &exe, const std::wstring &args, const std::wstring &dir, NativeWindow notify,
                          unsigned msg, std::uintptr_t tag, std::wstring *error)
{
	close();
	notify_ = notify;
	msg_ = msg;
	tag_ = tag;

	// the ends of the pipes are closed in a program started meanwhile (another engine); `fail` tells why the engine
	// did not start (it closes by itself when the engine starts)
	int in[2] = {-1, -1}, out[2] = {-1, -1}, fail[2] = {-1, -1};
	if (::pipe2(in, O_CLOEXEC) != 0 || ::pipe2(out, O_CLOEXEC) != 0 || ::pipe2(fail, O_CLOEXEC) != 0) {
		if (error) *error = L"pipe: " + widen(std::strerror(errno));
		close_all({in[0], in[1], out[0], out[1], fail[0], fail[1]});
		return false;
	}

	const std::string path = narrow(exe), folder = narrow(dir);
	std::vector<std::string> words = split_args(narrow(args));
	words.insert(words.begin(), path);
	std::vector<char *> argv;
	for (auto &w : words) argv.push_back(w.data());
	argv.push_back(nullptr);

	const pid_t pid = ::fork();
	if (pid < 0) {
		if (error) *error = L"fork: " + widen(std::strerror(errno));
		close_all({in[0], in[1], out[0], out[1], fail[0], fail[1]});
		return false;
	}
	if (pid == 0) {
		// (the child, of a program with other threads: only calls that are safe after a fork, up to the exec)
		::dup2(in[0], 0);
		::dup2(out[1], 1);
		::dup2(out[1], 2);
		int err = ::setpgid(0, 0) == 0 ? 0 : errno;
		if (!err && !folder.empty() && ::chdir(folder.c_str()) != 0) err = errno;
		if (!err) {
			::execv(path.c_str(), argv.data());
			err = errno;
		}
		const ssize_t w = ::write(fail[1], &err, sizeof err);
		(void)w;
		::_exit(127);
	}
	close_all({in[0], out[1], fail[1]});
	int err = 0;
	ssize_t got;
	do got = ::read(fail[0], &err, sizeof err);
	while (got < 0 && errno == EINTR);
	::close(fail[0]);
	if (got == static_cast<ssize_t>(sizeof err)) {
		int st = 0;
		::waitpid(pid, &st, 0);
		if (error) *error = widen(std::strerror(err));
		close_all({in[1], out[0]});
		return false;
	}
	pid_ = pid;
	reaped_ = false;
	status_ = -1;
	in_write_ = in[1];
	out_read_ = out[0];
	if (::fcntl(in_write_, F_SETFL, O_NONBLOCK) < 0 || ::fcntl(out_read_, F_SETFL, O_NONBLOCK) < 0) {
		if (error) *error = L"fcntl: " + widen(std::strerror(errno));
		close(0);
		return false;
	}
	stop_io_ = false;
	drain_output_ = false;
	writer_ended_ = false;
	read_ended_ = false;
	reader_ = std::thread([this] { read_loop(); });
	writer_ = std::thread([this] { write_loop(); });
	return true;
}

// (a write to an engine that has ended fails: the program ignores SIGPIPE, src/linux/main_linux.cpp and src/android)
bool EngineProcess::write_all(std::string_view text)
{
	while (!text.empty() && !stop_io_) {
		pollfd p{in_write_, POLLOUT, 0};
		const int ready = ::poll(&p, 1, 50);
		if (ready == 0 || (ready < 0 && errno == EINTR)) continue;
		if (ready < 0) return false;
		const ssize_t n = ::write(in_write_, text.data(), text.size());
		if (n < 0) {
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
			return false;
		}
		text.remove_prefix(static_cast<size_t>(n));
	}
	return true;
}

void EngineProcess::close_input()
{
	if (in_write_ >= 0) {
		::close(in_write_);
		in_write_ = -1;
	}
}

void EngineProcess::notify_write_failure()
{
	if (notify_) post_message(notify_, msg_, tag_, 2);
}

void EngineProcess::stop_writer()
{
	if (writer_.joinable()) {
		end_input();
		writer_.join();  // (the process has ended: a write the thread waits on fails)
	}
	close_input();  // (a start that failed before the thread)
	std::lock_guard lock(write_mutex_);
	to_write_.clear();
	input_closing_ = write_failed_ = false;
}

void EngineProcess::take_lines(std::vector<std::string> &out)
{
	std::lock_guard lock(mutex_);
	if (out.empty()) {
		out.swap(lines_);
	} else {
		for (auto &l : lines_) out.push_back(std::move(l));
		lines_.clear();
	}
}

void EngineProcess::read_loop()
{
	char buf[1 << 16];
	std::vector<std::string> batch;
	for (;;) {
		if (stop_io_) break;
		pollfd p{out_read_, POLLIN, 0};
		const int ready = ::poll(&p, 1, 50);
		if (ready == 0 && drain_output_) break;
		if (ready == 0 || (ready < 0 && errno == EINTR)) continue;
		if (ready < 0) break;
		const ssize_t got = ::read(out_read_, buf, sizeof buf);
		if (got < 0 && errno == EINTR) continue;
		if (got <= 0) break;
		const size_t n = static_cast<size_t>(got);
		size_t i = 0;
		for (size_t k = 0; k < n; ++k) {
			// a line ends with '\n'; the progress of the book commands is rewritten after '\r'
			if (buf[k] != '\n' && buf[k] != '\r') continue;
			partial_.append(buf + i, k - i);
			i = k + 1;
			add_line(batch, partial_);
			partial_.clear();
		}
		partial_.append(buf + i, n - i);
		if (!batch.empty()) {
			bool was_empty;
			{
				std::lock_guard lock(mutex_);
				was_empty = lines_.empty();
				for (auto &l : batch) lines_.push_back(std::move(l));
			}
			batch.clear();
			if (was_empty && notify_) post_message(notify_, msg_, tag_, 0);
		}
	}
	// the last line, when the engine ended without ending it (its error, as Edax without its eval file)
	add_line(batch, partial_);
	partial_.clear();
	if (!batch.empty()) {
		std::lock_guard lock(mutex_);
		for (auto &l : batch) lines_.push_back(std::move(l));
	}
	read_ended_ = true;
	if (notify_) post_message(notify_, msg_, tag_, 1);  // the output has ended
}

namespace {

// the process has ended (and is reaped, its status kept: -1 when it is not known); false while it runs
bool ended(int pid, bool *reaped, int *status)
{
	if (*reaped) return true;
	int st = 0;
	siginfo_t ended_child{};
	if (::waitid(P_PID, static_cast<id_t>(pid), &ended_child, WEXITED | WNOHANG | WNOWAIT) == 0) {
		if (ended_child.si_pid == 0) return false;
		::kill(-pid, SIGKILL);  // release the engine's inherited pipes before reaping the leader
	} else if (errno != ECHILD) {
		return false;
	}
	const pid_t r = ::waitpid(pid, &st, WNOHANG);
	if (r == pid || (r < 0 && errno == ECHILD)) {
		*reaped = true;
		*status = r == pid ? st : -1;
	}
	return *reaped;
}

}  // namespace

void EngineProcess::quit()
{
	if (pid_ > 0 && !ended(pid_, &reaped_, &status_)) send("quit\n");  // (once: the input then closes)
	end_input();
}

bool EngineProcess::exited() const
{
	if (pid_ <= 0) return true;
	// (its last lines may be still in the pipe a moment after its end)
	auto *self = const_cast<EngineProcess *>(this);
	if (!ended(pid_, &self->reaped_, &self->status_)) return false;
	drain_output_ = true;
	return read_ended_ || !reader_.joinable();
}

std::optional<int> EngineProcess::exit_code() const
{
	auto *self = const_cast<EngineProcess *>(this);
	if (pid_ <= 0 || !ended(pid_, &self->reaped_, &self->status_) || status_ < 0) return std::nullopt;
	if (WIFEXITED(status_)) return WEXITSTATUS(status_);
	return 128 + (WIFSIGNALED(status_) ? WTERMSIG(status_) : 0);  // (ended by a signal)
}

void EngineProcess::close(unsigned wait_ms)
{
	quit();
	if (pid_ > 0) {
		const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
		while (!ended(pid_, &reaped_, &status_) && std::chrono::steady_clock::now() < until) ::usleep(10 * 1000);
		if (!ended(pid_, &reaped_, &status_)) {
			::kill(-pid_, SIGKILL);
			for (int i = 0; i < 500 && !ended(pid_, &reaped_, &status_); ++i) ::usleep(10 * 1000);
		}
	}
	stop_io_ = true;
	stop_writer();
	if (reader_.joinable()) reader_.join();  // (the pipe closes when the process ends; the thread then stops)
	if (out_read_ >= 0) {
		::close(out_read_);
		out_read_ = -1;
	}
	pid_ = -1;
	reaped_ = false;
	status_ = -1;
	partial_.clear();
	std::lock_guard lock(mutex_);
	lines_.clear();
}

}  // namespace ng
