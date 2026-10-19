#include "engine.h"

namespace ng {

bool EngineProcess::start(const std::wstring &exe, const std::wstring &args, const std::wstring &dir, HWND notify,
                          UINT msg, WPARAM tag, std::wstring *error)
{
	close();
	notify_ = notify;
	msg_ = msg;
	tag_ = tag;

	SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
	HANDLE in_read = nullptr, out_write = nullptr;
	if (!CreatePipe(&in_read, &in_write_, &sa, 0) || !CreatePipe(&out_read_, &out_write, &sa, 1 << 16)) {
		if (error) *error = L"CreatePipe";
		close();
		return false;
	}
	SetHandleInformation(in_write_, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(out_read_, HANDLE_FLAG_INHERIT, 0);

	// only the three pipe ends are inherited
	SIZE_T size = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
	std::vector<char> attr_buf(size);
	auto *attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buf.data());
	InitializeProcThreadAttributeList(attrs, 1, 0, &size);
	HANDLE inherit[2] = {in_read, out_write};
	UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof inherit, nullptr, nullptr);

	STARTUPINFOEXW si{};
	si.StartupInfo.cb = sizeof si;
	si.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.StartupInfo.wShowWindow = SW_HIDE;
	si.StartupInfo.hStdInput = in_read;
	si.StartupInfo.hStdOutput = out_write;
	si.StartupInfo.hStdError = out_write;
	si.lpAttributeList = attrs;

	std::wstring cmd = L"\"" + exe + L"\" " + args;
	PROCESS_INFORMATION pi{};
	const BOOL ok = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE,
	                               CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr,
	                               dir.empty() ? nullptr : dir.c_str(), &si.StartupInfo, &pi);
	const DWORD err = GetLastError();
	DeleteProcThreadAttributeList(attrs);
	CloseHandle(in_read);
	CloseHandle(out_write);
	if (!ok) {
		if (error) {
			wchar_t buf[256] = L"";
			FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0, buf, 256, nullptr);
			*error = buf;
		}
		close();
		return false;
	}
	// the engine ends with the GUI, even when the GUI ends without closing it
	job_ = CreateJobObjectW(nullptr, nullptr);
	if (job_) {
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
		li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &li, sizeof li);
		AssignProcessToJobObject(job_, pi.hProcess);
	}
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	process_ = pi.hProcess;
	pid_ = pi.dwProcessId;
	reader_ = std::thread([this] { read_loop(); });
	return true;
}

bool EngineProcess::exited() const
{
	return process_ && WaitForSingleObject(process_, 0) == WAIT_OBJECT_0;
}

bool EngineProcess::send(std::string_view text)
{
	if (!in_write_) return false;
	DWORD done = 0;
	while (!text.empty()) {
		if (!WriteFile(in_write_, text.data(), static_cast<DWORD>(text.size()), &done, nullptr)) return false;
		text.remove_prefix(done);
	}
	return true;
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
		DWORD n = 0;
		if (!ReadFile(out_read_, buf, sizeof buf, &n, nullptr) || n == 0) break;
		size_t i = 0;
		for (DWORD k = 0; k < n; ++k) {
			if (buf[k] != '\n') continue;
			partial_.append(buf + i, k - i);
			i = k + 1;
			if (!partial_.empty() && partial_.back() == '\r') partial_.pop_back();
			// the engine's prompt ">" may come before a line
			size_t a = 0;
			while (a < partial_.size() && (partial_[a] == '>' || partial_[a] == ' ' || partial_[a] == '\r')) ++a;
			if (a < partial_.size()) batch.emplace_back(partial_.substr(a));
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
			if (was_empty && notify_) PostMessageW(notify_, msg_, tag_, 0);
		}
	}
	if (notify_) PostMessageW(notify_, msg_, tag_, 1);  // the output has ended
}

void EngineProcess::close(DWORD wait_ms)
{
	if (process_) {
		if (in_write_ && WaitForSingleObject(process_, 0) == WAIT_TIMEOUT) send("quit\n");
		if (in_write_) {
			CloseHandle(in_write_);
			in_write_ = nullptr;
		}
		if (WaitForSingleObject(process_, wait_ms) == WAIT_TIMEOUT) {
			TerminateProcess(process_, 1);
			WaitForSingleObject(process_, 5000);
		}
	}
	if (in_write_) {
		CloseHandle(in_write_);
		in_write_ = nullptr;
	}
	if (reader_.joinable()) {
		// the pipe closes when the process ends; the thread then stops
		if (!process_) CancelSynchronousIo(reader_.native_handle());
		reader_.join();
	}
	if (out_read_) {
		CloseHandle(out_read_);
		out_read_ = nullptr;
	}
	if (process_) {
		CloseHandle(process_);
		process_ = nullptr;
	}
	if (job_) {
		CloseHandle(job_);
		job_ = nullptr;
	}
	pid_ = 0;
	partial_.clear();
	std::lock_guard lock(mutex_);
	lines_.clear();
}

}  // namespace ng
