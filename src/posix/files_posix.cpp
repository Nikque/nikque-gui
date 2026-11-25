// The files and the names of src/platform.h on Linux and Android (POSIX): names are UTF-8, folders are separated by '/'.
#include "../platform.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <system_error>

#include "../ini.h"
#include "posix.h"

namespace ng {

namespace {

std::string u8(const std::wstring &w) { return narrow(w); }

}  // namespace

std::wstring path_join(const std::wstring &dir, std::wstring_view rel)
{
	std::wstring r(rel);
	for (auto &c : r)
		if (c == L'\\') c = L'/';
	if (!dir.empty() && dir.back() == L'/') return dir + r;
	return dir + L"/" + r;
}

bool is_full_path(std::wstring_view p) { return !p.empty() && p[0] == L'/'; }

FILE *open_file(const std::wstring &path, const wchar_t *mode)
{
	std::string m;
	for (const wchar_t *p = mode; *p; ++p) m += static_cast<char>(*p);
	return std::fopen(u8(path).c_str(), m.c_str());
}

bool delete_file(const std::wstring &path) { return ::unlink(u8(path).c_str()) == 0; }

bool move_file(const std::wstring &from, const std::wstring &to, bool copy_allowed)
{
	const std::string a = u8(from), b = u8(to);
	if (::rename(a.c_str(), b.c_str()) == 0) return true;
	if (errno != EXDEV || !copy_allowed) return false;
	// another file system: copied next to `to` first, and the copy renamed over it (`to` is never left half written).
	// false when `from` cannot be taken away after: `to` then holds the copy, and `from` stays too.
	const std::string part = b + ".part" + std::to_string(::getpid());
	std::error_code ec;
	std::filesystem::copy_file(a, part, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec || ::rename(part.c_str(), b.c_str()) != 0) {
		::unlink(part.c_str());
		return false;
	}
	return ::unlink(a.c_str()) == 0;
}

bool copy_file(const std::wstring &from, const std::wstring &to)
{
	std::error_code ec;
	std::filesystem::copy_file(u8(from), u8(to), std::filesystem::copy_options::overwrite_existing, ec);
	return !ec;
}

bool make_dir(const std::wstring &path) { return ::mkdir(u8(path).c_str(), 0755) == 0; }

bool path_exists(const std::wstring &path)
{
	struct stat st;
	return ::stat(u8(path).c_str(), &st) == 0;
}

bool dir_exists(const std::wstring &path)
{
	struct stat st;
	return ::stat(u8(path).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool file_exists(const std::wstring &path)
{
	struct stat st;
	return ::stat(u8(path).c_str(), &st) == 0 && !S_ISDIR(st.st_mode);
}

std::optional<std::uint64_t> file_size(const std::wstring &path)
{
	struct stat st;
	if (::stat(u8(path).c_str(), &st) != 0) return std::nullopt;
	return static_cast<std::uint64_t>(st.st_size);
}

int seek64(FILE *f, long long at, int whence) { return ::fseeko(f, static_cast<off_t>(at), whence); }

long long tell64(FILE *f) { return static_cast<long long>(::ftello(f)); }

std::optional<std::wstring> decode_text(std::string_view bytes, unsigned code_page)
{
	if (code_page == 65001) {
		// strict UTF-8: no byte that does not make a letter
		const std::wstring w = widen(bytes);
		if (w.find(static_cast<wchar_t>(0xfffd)) != std::wstring::npos && narrow(w) != bytes) return std::nullopt;
		return w;
	}
	return decode_codepage(bytes, code_page);
}

bool RandomFile::open(const std::wstring &path)
{
	close();
	fd_ = ::open(u8(path).c_str(), O_RDONLY | O_CLOEXEC);
	return fd_ >= 0;
}

void RandomFile::close()
{
	if (fd_ >= 0) ::close(fd_);
	fd_ = -1;
}

bool RandomFile::is_open() const noexcept { return fd_ >= 0; }

std::optional<std::uint64_t> RandomFile::size() const
{
	struct stat st;
	if (fd_ < 0 || ::fstat(fd_, &st) != 0) return std::nullopt;
	return static_cast<std::uint64_t>(st.st_size);
}

bool RandomFile::read(std::uint64_t at, unsigned char *out, size_t size, size_t *got)
{
	size_t done = 0;
	while (done < size) {
		const ssize_t n = ::pread(fd_, out + done, size - done, static_cast<off_t>(at + done));
		if (n < 0) {
			if (errno == EINTR) continue;
			return false;
		}
		if (n == 0) break;
		done += static_cast<size_t>(n);
	}
	*got = done;
	return true;
}

// (the names of the files keep the case of their letters: the same file is the same file of the system, or the same name)
bool same_file(const std::wstring &a, const std::wstring &b)
{
	std::error_code ec;
	if (std::filesystem::equivalent(u8(a), u8(b), ec) && !ec) return true;
	return a == b;
}

unsigned process_id() { return static_cast<unsigned>(::getpid()); }

int cpu_count()
{
	const long n = ::sysconf(_SC_NPROCESSORS_ONLN);
	return n > 0 ? static_cast<int>(n) : 1;
}

void sleep_ms(unsigned ms) { ::usleep(ms * 1000); }

}  // namespace ng
